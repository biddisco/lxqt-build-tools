#include <QObject>
#include <QString>
//
#include <string>
//
#include <ripple/protocol/Sign.h>
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
//
#include "src/order_book.hpp"
#include "src/settings.hpp"
#include "src/currency_widget.hpp"
#include "src/exchange/xrpl_network.hpp"
#include "src/exchange/bitstamp.hpp"
#include "src/exchange/xrpl.hpp"
//
//#include <test/jtx.h>
#include <test/jtx/WSClient.h>
#define line_string "# ---------------------------------\n"

// ----------------------------------------------------------------------------
xrpl_network::xrpl_network(bool testnet) : testnet_(testnet)
{
    belle_https_ripple.address(dataapi_address());
    belle_https_ripple.port(dataapi_port());
    belle_https_ripple.ssl(true);
    // set the http 'on error' callback
    belle_https_ripple.on_http_error([](auto& ctx)
    {
      std::cerr << "(belle_https_ripple) : Error: " << ctx.ec.message() << "\n\n";
    });

    belle_jsonrpc_network.address(jsonrpc_address());
    belle_jsonrpc_network.port(jsonrpc_port());
    belle_jsonrpc_network.ssl(true);
    // set the http 'on error' callback
    belle_jsonrpc_network.on_http_error([](auto& ctx)
    {
      std::cerr << "(belle_jsonrpc_network) : Error: " << ctx.ec.message() << "\n\n";
    });
}

// ----------------------------------------------------------------------------
xrpl_network::~xrpl_network()
{
    qDebug() << "xrpl_network: destructor" << " testnet " << testnet();
    delete orderbook_;
}

// ----------------------------------------------------------------------------
void xrpl_network::set_plot(OrderBookPlot *obp)
{
    orderbook_ = new xrpl_order_book(obp, true);
}

// ----------------------------------------------------------------------------
bool xrpl_network::testnet() const
{
    return testnet_;
}

// ----------------------------------------------------------------------------
std::string xrpl_network::network_address() const {
    if (testnet_) return ripple_testnet_address;
    return ripple_mainnet_address;
}
std::string xrpl_network::jsonrpc_address() const {
    if (testnet_) return ripple_jsonrpc_testaddr;
    return ripple_jsonrpc_address;
}
int xrpl_network::network_port() const {
    if (testnet_) return ripple_testnet_port;
    return ripple_mainnet_port;
}
int xrpl_network::jsonrpc_port() const {
    if (testnet_) return ripple_jsonrpc_port;
    return ripple_jsonrpc_testport;
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
bool xrpl_network::can_send(currency &c, exchange *dest) {
    // yes to anything if the source is also an xrpl wallet
    if (dynamic_cast<xrpl_network*>(dest)) {
        return dynamic_cast<xrpl_network*>(dest)->testnet()==testnet();
    }
    else if (!testnet() && dynamic_cast<bitstamp_network*>(dest)) {
        if (c.type_==currency_type::xrp || c.type_==currency_type::usd_bitstamp || c.type_==currency_type::eur_bitstamp) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
const xrpl_order_book &xrpl_network::get_orderbook() const
{
    return *orderbook_;
}

// ----------------------------------------------------------------------------
void xrpl_network::connect(net::contexts &/*io_contexts*/)
{
    // @TODO - implement something useful here
}

// ----------------------------------------------------------------------------
void xrpl_network::disconnect()
{
    qDebug() << "xrpl_network: websockets: shutdown start" << " testnet " << testnet();

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
            std::cerr << "Account balance : HTTPS Error: " << ctx.res.result_int()
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

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_infos()
{
    /*
    {
      "method": "account_info",
      "params": [
        {
          "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
          "strict": true,
          "ledger_index": "current",
          "queue": true
        }
      ]
    }
    */

    for (auto &w : subscribed_wallets_) {
        nlohmann::json params;
        params["account"] = w.public_;
        params["ledger_index"] = "current";
        params["strict"] = true;
        params["queue"] = true;

        nlohmann::json content;
        content["method"] = "account_info";
        content["params"] = nlohmann::json::array({params});

        using namespace OB;
        // init an http request object
        Belle::Request req;

        // set the method
        req.method(Belle::Method::post);
        req.set(Belle::Header::host, network_address());
        req.set(Belle::Header::user_agent, "mystery");
        req.set(Belle::Header::content_type, "application/json");
        req.set(Belle::Header::accept, "application/json");
        req.set(Belle::Header::connection, "close");
        // set the target path
        req.target("/");
        req.body() = content.dump();
        req.prepare_payload();
        DEBUG_ONLY(line_string << req);

        belle_jsonrpc_network.on_http(req, [this, &w](auto& ctx)
        {
          if (ctx.res.result() != OB::Belle::Status::ok)
          {
            std::cerr << "account_info : " << w.public_ << " : HTTPS Error: " << ctx.res.result_int()
                      << " " << ctx.res.reason()
                      << "\n";
            return;
          }
          // debug : print the response headers and body
          DEBUG_ONLY(line_string << "account_info response : " << ctx.res.body());
          this->handle_account_info(w, std::move(ctx.res.body()));
        });
        belle_jsonrpc_network.connect();
    }
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_info(ledger_wallet &w, std::string&& data)
{
    nlohmann::json jdata = json::parse(data)["result"]["account_data"];
    DEBUG_ONLY(jdata.dump(4));
    //
    if (jdata.is_null()) return;
    //
    assert(w.public_ == jdata.at("Account").get< std::string >());
    w.sequence_ = jdata.at("Sequence").get< int32_t >();
    DEBUG_ALWAYS(w.public_ << " Sequence " << w.sequence_);
    // signal GUI to update
    emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
bool xrpl_network::make_payment(currency &c, basic_account *src, basic_account *dest)
{
    ledger_wallet *from = static_cast<ledger_wallet*>(src);
    ledger_wallet *to = static_cast<ledger_wallet*>(dest);
    std::cout << "XRPL payment amount " << c.balance_
              << " from " << from->public_
              << " to " << to->public_
              << ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : "") << std::endl;

    std::string signed_tx = make_xrp_payment(
            ripple::KeyType::secp256k1,
            from->private_,
            from->public_,
            from->sequence_,
            to->public_,
            to->tag_,
            static_cast<uint64_t>(c.balance_*1000000));

    from->sequence_++;

    nlohmann::json tx;
    tx["tx_blob"] = signed_tx;

    nlohmann::json content;
    content["method"] = "submit";
    content["params"] = nlohmann::json::array({tx});

    using namespace OB;
    // init an http request object
    Belle::Request req;

    // set the method
    req.method(Belle::Method::post);
    req.set(Belle::Header::host, network_address());
    req.set(Belle::Header::user_agent, "mystery");
    req.set(Belle::Header::content_type, "application/json");
    req.set(Belle::Header::accept, "application/json");
    req.set(Belle::Header::connection, "close");
    // set the target path
    req.target("/");
    req.body() = content.dump();
    req.prepare_payload();
    DEBUG_ONLY(line_string << req);

    belle_jsonrpc_network.on_http(req, [this](auto& ctx)
    {
      if (ctx.res.result() != OB::Belle::Status::ok)
      {
        std::cerr << "Make payment : HTTPS Error: " << ctx.res.result_int()
                  << " " << ctx.res.reason()
                  << "\n";
        return;
      }
      // debug : print the response headers and body
      DEBUG_ONLY("Tx submit response " << ctx.res.body() << "\n");
//      this->handle_account_balance(w, std::move(ctx.res.body()));
    });

    belle_jsonrpc_network.connect();


//    {
//        using namespace std::chrono_literals;
//        using namespace jtx;
//        Basic_Con
//        Env env(*this);
//        env.fund(XRP(10000), "alice", "bob");
//        env.close();
//        auto wsc = makeWSClient(env.app().config());

//    }

    return true;
}

// ----------------------------------------------------------------------------
// this function not yet working
#if 0

void on_http_error(OB::Belle::Client& belle_https_connection)
{
  // set the http on error callback
  belle_https_connection.on_http_error([](auto& ctx)
  {
    std::cerr << "Error: " << ctx.ec.message() << "\n\n";
  });
}

void GroxMainWindow::ledger_order_book(bool buy_xrp)
{
    // curl command to query : buy xrp for USD.bitstamp
    // curl -H 'Content-Type: application/json' -d '{"method":"book_offers","params":[{"taker_gets":{"currency":"XRP"},"taker_pays":{"currency":"USD","issuer":"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"},"limit":10}]}' https://s1.ripple.com:51234/
    // "{ \"command\": \"subscribe\", \"books\": [ { \"taker_pays\": { \"currency\": \"XRP\" }, \"taker_gets\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true }, { \"taker_gets\": { \"currency\": \"XRP\" }, \"taker_pays\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true } ] }",

    // init client with remote address, port, and ssl enabled
    OB::Belle::Client app{ripple_network_address, ripple_network_port, true};
    on_http_error(app);
/*
{
  "command": "subscribe",
  "books": [
    {
      "taker_pays": {
        "currency": "XRP"
      },
      "taker_gets": {
        "currency": "USD",
        "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
      },
      "snapshot": true
    },
    {
      "taker_gets": {
        "currency": "XRP"
      },
      "taker_pays": {
        "currency": "USD",
        "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
      },
      "snapshot": true
    }
  ]
}
*/

    // init an http request object
    Belle::Request req;

    nlohmann::json content;
    content["command"] = "subscribe";

    // order to buy XRP : the taker of this offer will pay XRP for my USD
    nlohmann::json buy;
    buy["taker_pays"]["currency"] = "XRP";
    buy["taker_gets"]["currency"] = "USD";
    buy["taker_gets"]["issuer"]   = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    buy["snapshot"] = "true";

    nlohmann::json sell;
    // order to sell XRP : the taker of this offer will pay USD for my XRP
    sell["taker_gets"]["currency"] = "XRP";
    sell["taker_pays"]["currency"] = "USD";
    sell["taker_pays"]["issuer"]   = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    sell["snapshot"] = "true";

    content["books"] = nlohmann::json::array({buy,sell});

    // set the method
    req.method(Belle::Method::post);
    req.set(Belle::Header::host, ripple_network_address);
    req.set(Belle::Header::user_agent, "mystery");
    req.set(Belle::Header::content_type, "application/json");
    req.set(Belle::Header::accept, "application/json");
    req.set(Belle::Header::connection, "close");
    // set the target path
    req.target("/");
    req.body() = content.dump();
    req.prepare_payload();

    app.on_http(req.move(), [this, buy_xrp](auto& ctx) {
        // check http status code
        if (ctx.res.result() != Belle::Status::ok)
        {
            // print the response status code and reason
            std::cerr << "Error: " << ctx.res.result_int() << " " << ctx.res.reason()
                      << "\n\n";
            return;
        }
        // debug : print the response headers and body
        DEBUG_ALWAYS("Request response " << ctx.res.body());
//        if (buy_xrp)
//            this->ledger_book_buy_xrp(std::move(ctx.res.body()));
//        else
//            this->ledger_book_sell_xrp(std::move(ctx.res.body()));
    });

    // save the number of requests in the queue
    auto total = app.queue().size();

    // start the client and save the number of completed requests
    auto completed = app.connect();
    DEBUG_ONLY("Completed " << completed);
}
#endif
