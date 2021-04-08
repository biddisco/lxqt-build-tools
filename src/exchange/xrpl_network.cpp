#include <QObject>
#include <QString>
//
#include <string>
//
#include "src/internet/https-async.hpp"
#include "src/internet/websocket-ssl.hpp"
#include "src/internet/evp-encrypt.hpp"
//
#include "../order_book.hpp"
#include "../settings.hpp"
#include "../currency_widget.hpp"
#include "xrpl_network.hpp"

// ----------------------------------------------------------------------------
xrpl_network::xrpl_network(bool testnet) : testnet_(testnet)
{
    belle_https_ripple.address(dataapi_address());
    belle_https_ripple.port(dataapi_port());
    belle_https_ripple.ssl(true);

    // set the http 'on error' callback
    belle_https_ripple.on_http_error([](auto& ctx)
    {
      std::cerr << "Error: " << ctx.ec.message() << "\n\n";
    });
}

// ----------------------------------------------------------------------------
void xrpl_network::set_plot(OrderBookPlot *obp) {
    plot_ = obp;
    orderbook_ = new xrpl_order_book(obp, true);
}

// ----------------------------------------------------------------------------
bool xrpl_network::testnet() const {
    return testnet_;
}

// ----------------------------------------------------------------------------
std::string xrpl_network::network_address() const {
    if (testnet_) return ripple_testnet_address;
    return ripple_mainnet_address;
}
int xrpl_network::network_port() const {
    if (testnet_) return ripple_testnet_port;
    return ripple_mainnet_port;
}
std::string xrpl_network::dataapi_address() const {
    if (testnet_) return ripple_testapi_address;
    return ripple_dataapi_address;
}
int xrpl_network::dataapi_port() const {
    if (testnet_) return ripple_testapi_port;
    return ripple_dataapi_port;
}

// ----------------------------------------------------------------------------
bool xrpl_network::can_send(currency &/*c*/, exchange *dest) {
    // yes to anything if the source is also an xrpl wallet
    if (dynamic_cast<xrpl_network*>(dest)) {
        return dynamic_cast<xrpl_network*>(dest)->testnet()==testnet();
    }
    return false;
}

// ----------------------------------------------------------------------------
xrpl_order_book *xrpl_network::get_orderbook()
{
    return orderbook_;
}

// ----------------------------------------------------------------------------
void xrpl_network::connect()
{
    // @TODO - implement something useful here
}

// ----------------------------------------------------------------------------
void xrpl_network::disconnect()
{
    if (ws_orderbook) {
        ws_orderbook->shutdown_blocking();
        ws_orderbook.reset();
    }
    if (ws_accounts) {
        ws_accounts->shutdown_blocking();
        ws_accounts.reset();
    }
}

// ----------------------------------------------------------------------------
void xrpl_network::add_wallet(const ledger_wallet &w)
{
    subscribed_wallets_.push_back(w);
}

// ----------------------------------------------------------------------------
void xrpl_network::subscribe_orderbook(net::contexts &io_contexts) {
    using namespace std::placeholders;
    ws_orderbook = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
      network_address(), std::to_string(network_port()),
      "{ \"command\": \"subscribe\", \"books\": [ { \"taker_pays\": { \"currency\": \"XRP\" }, \"taker_gets\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true }, { \"taker_gets\": { \"currency\": \"XRP\" }, \"taker_pays\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true } ] }",
      std::bind(xrpl_network::new_order_data, this, _1));
}

// ----------------------------------------------------------------------------
void xrpl_network::subscribe_accounts(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    std::string addresses;
    for (const auto &w: subscribed_wallets_) {
        if (addresses.size()) addresses += ", ";
        addresses += "\"" + w.public_ + "\"";
    }
    std::string subscription =
            "{ \"command\": \"subscribe\", \"accounts\": [ " + addresses + " ] }";
    DEBUG_ONLY("Subscribing to account changes for \n" << addresses);

    ws_accounts = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
      network_address(), std::to_string(network_port()), subscription,
      std::bind(new_account_data, this, _1));
}

// ----------------------------------------------------------------------------
void xrpl_network::new_order_data(xrpl_network* nw, std::string_view data)
{
    if (startswith(data, "{\"result\":")) {
        nw->orderbook_->accept_json_ledger_snapshot(data);
    }
    else if (startswith(data, "{\"engine_result\":")) {
        nw->orderbook_->accept_json_ledger_transaction(data);
    }

    // Send signal to mainwindow
    QString datastring = QString::fromStdString(nw->orderbook_->order_text);
    emit nw->new_order_book_data(datastring);
}

// ----------------------------------------------------------------------------
void xrpl_network::new_account_data(xrpl_network* nw, std::string_view data)
{
    if (startswith(data, "{\"result\":")) {
        // ignore this, just a subscription ok
        DEBUG_ONLY("Account subscription : " << data);
    }
    else if (startswith(data, "{\"engine_result\":\"tesSUCCESS\"")) {
        nlohmann::json jdata = json::parse(data)["meta"]["AffectedNodes"];
        DEBUG_ONLY("Account changes : " << jdata.dump(4));
        for (const auto &j : jdata) {
            auto m = j["ModifiedNode"];
            auto f = m["FinalFields"];
            auto p = m["PreviousFields"];
            //
            std::string acct = f["Account"].get<std::string>();
            double oldb = 1E-6*std::stod(p["Balance"].get<std::string>());
            double newb = 1E-6*std::stod(f["Balance"].get<std::string>());
            std::cout << "Acct " << acct << " old balance " << oldb << " new balance " << newb << std::endl;
            nw->update_balance(acct, oldb, newb);
        }
    }
}

// ----------------------------------------------------------------------------
void xrpl_network::update_balance(std::string_view addr, double oldb, double newb)
{
    auto it = ranges::find_if(subscribed_wallets_, [addr](ledger_wallet const &w){
        return w.public_ == addr;
    });
    if (it==subscribed_wallets_.end()) {
        std::cerr << "Balance update did not find acct " << addr << std::endl;
        return;
    }
    auto &c_list = it->currencies_;
    auto it2 = ranges::find_if(c_list, [](currency const &c){
        return c.type_ == currency_type::xrp;
    });
    if (it2==c_list.end()) {
        std::cerr << "Balance update did not find ledger currency" << std::endl;
        return;
    }
    if (it2->balance_ != oldb) {
        std::cerr << "Old balance error " << it2->balance_ << " expected " << oldb << std::endl;
    }
    std::cerr << "Balance updated from " << oldb << " to " << newb << std::endl;
    it2->balance_ = newb;
    it2->avail_ = newb - it2->reserved_;

    // signal GUI to update
    emit update_currency_widget(&(*it2));
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_balances()
{
    for (auto &w : subscribed_wallets_) {
        std::string target = "/v2/accounts/" + w.public_ + "/balances";

        belle_https_ripple.on_http(target, [this, &w](auto& ctx)
        {
          if (ctx.res.result() != OB::Belle::Status::ok)
          {
            std::cerr << "HTTPS Error: " << ctx.res.result_int()
                      << " " << ctx.res.reason()
                      << " - Address Not found? " << w.public_ << "\n";
            return;
          }
          // debug : print the response headers and body
          DEBUG_ONLY("Ledger response " << ctx.res.body() << "\n");
          this->handle_account_balance(w, std::move(ctx.res.body()));
        });
    }
    belle_https_ripple.connect();
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_balance(ledger_wallet &w, std::string&& data)
{
    nlohmann::json jdata = json::parse(data)["balances"];
    DEBUG_ONLY(jdata.dump(4));
    std::vector<xrp_amount> balances = jdata.get<std::vector<xrp_amount>>();
    //
    for (const auto &b : balances) {
        // @TODO, do not hardcode USD
        if (b.currency == currency_type::usd_bitstamp) {
            currency c{"USD", "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B", currency_type::usd_bitstamp, b.value, b.value, 0, nullptr};
            add_currency(c, w.currencies_);
        }
        else if (b.currency == currency_type::eur_bitstamp) {
            currency c{"EUR", "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B", currency_type::eur_bitstamp, b.value, b.value, 0, nullptr};
            add_currency(c, w.currencies_);
        }
        else if (b.currency == currency_type::xrp) {
            currency c{"XRP", "", currency_type::xrp, b.value, b.value, 0, nullptr};
            add_currency(c, w.currencies_);
        }
    }

    // signal GUI to update
    emit update_wallet_widget(&w);
}
