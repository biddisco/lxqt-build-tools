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
std::vector<std::pair<currency_type, currency_type>> xrpl_network::currency_pairs()
{
    std::vector<std::pair<currency_type, currency_type>> supported = {
        {usd_bitstamp,xrp},
        {xrp,usd_bitstamp},
    };
    return supported;
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
void xrpl_network::subscribe_orderbook(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    //
    nlohmann::json command;
    command["command"] = "subscribe";
    // buying xrp
    nlohmann::json buy_xrp;
    buy_xrp["taker_gets"]["currency"] = "XRP";
    buy_xrp["taker_pays"]["currency"] = "USD";
    buy_xrp["taker_pays"]["issuer"] = currency::bitstamp_trust;
    buy_xrp["snapshot"] = true;
    // selling xrp
    nlohmann::json sell_xrp;
    sell_xrp["taker_pays"]["currency"] = "XRP";
    sell_xrp["taker_gets"]["currency"] = "USD";
    sell_xrp["taker_gets"]["issuer"] = currency::bitstamp_trust;
    sell_xrp["snapshot"] = true;
    // subscribe to 2 books
    command["books"] = nlohmann::json::array({buy_xrp, sell_xrp});
    std::string subscription = command.dump();
    DEBUG_ONLY("JSON text is : " << subscription);
    DEBUG_ALWAYS("Subscribing to xrpl:XRP/USD orderbook");

    ws_orderbook = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
      network_address(), std::to_string(network_port()), subscription,
      std::bind(xrpl_network::new_orderbook_data, this, _1));
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
    DEBUG_ALWAYS("Subscribing to account changes for \n" << addresses);

    ws_accounts = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
      network_address(), std::to_string(network_port()), subscription,
      std::bind(xrpl_network::new_account_data, this, _1));
}

// ----------------------------------------------------------------------------
void xrpl_network::new_orderbook_data(xrpl_network* nw, std::string_view data)
{
    DEBUG_ONLY("xrpl new_orderbook_data : thread " << std::this_thread::get_id());

    if (startswith(data, "{\"result\":")) {
        nw->orderbook_->accept_json_ledger_snapshot(data);
    }
    else if (startswith(data, "{\"engine_result\":")) {
        nw->orderbook_->accept_json_ledger_transaction(data);
    }

    // Send signal to mainwindow
    emit nw->orderbook_changed();
}

// ----------------------------------------------------------------------------
void xrpl_network::new_account_data(xrpl_network* nw, std::string_view data)
{
    DEBUG_ONLY("xrpl account_data : thread " << std::this_thread::get_id());
    DEBUG_ONLY("Account changes : " << data);
    if (startswith(data, "{\"result\":")) {
        // ignore this, just a subscription ok
        DEBUG_ONLY("Account subscription : " << data);
    }
    else if (startswith(data, "{\"engine_result\":\"tesSUCCESS\"")) {
        nlohmann::json jdata = json::parse(data);
        DEBUG_ONLY("Account changes : " << jdata.dump(4));
        nlohmann::json adata = jdata["meta"]["AffectedNodes"];
        for (const auto &a : adata) {
            try {
                auto m = a["ModifiedNode"];
                auto f = m["FinalFields"];
                auto p = m["PreviousFields"];
                auto b = f["Balance"];
                double oldb;
                double newb;
                //
                if (f.contains("Account")) {
                    std::string acct = f["Account"].get<std::string>();
                    oldb = 1E-6*std::stod(p["Balance"].get<std::string>());
                    newb = 1E-6*std::stod(b.get<std::string>());
                    std::cout << "Acct " << acct
                              << " old balance " << oldb
                              << " new balance " << newb << std::endl;
                    nw->update_XRP_balance(acct, oldb, newb);
                }
                // is this an IOU balance change?
                else if (b.contains("issuer") && (b["issuer"].get<std::string>() == "rrrrrrrrrrrrrrrrrrrrBZbvji")) {
                    currency curr;
                    auto t              = jdata["transaction"];
                    std::string fm_acct = t["Account"];
                    std::string to_acct = t["Destination"];
                    curr.name_          = b["currency"].get<std::string>();
                    curr.issuer_        = t["SendMax"]["issuer"].get<std::string>();
                    curr.balance_       = std::stod(b["value"].get<std::string>());
                    curr.type_          = get_currency_type(curr.name_, curr.issuer_);
                    if (to_acct == f["LowLimit"]["issuer"].get<std::string>()) {
                        std::cout << "Acct " << to_acct
                                  << " IOU balance change " << curr.balance_ << std::endl;
                        nw->update_IOU_balance(to_acct, curr);
                    }
                    if (fm_acct == f["HighLimit"]["issuer"].get<std::string>()) {
                        curr.balance_ = -curr.balance_;
                        std::cout << "Acct " << fm_acct
                                  << " IOU balance change " << curr.balance_ << std::endl;
                        nw->update_IOU_balance(fm_acct, curr);
                    }
                }
            }
            catch (...) {
                DEBUG_ALWAYS("Account changes : " << data);
            }
        }
    }
}

// ----------------------------------------------------------------------------
std::vector<currency>::iterator xrpl_network::get_currency(std::string_view addr, currency_type t)
{
    auto it = ranges::find_if(subscribed_wallets_, [addr](ledger_wallet const &w){
        return w.public_ == addr;
    });
    if (it==subscribed_wallets_.end()) {
        std::cerr << "get currency did not find acct " << addr << std::endl;
        return std::vector<currency>::iterator(nullptr);
    }
    auto &c_list = it->currencies_;
    auto it2 = ranges::find_if(c_list, [t](currency const &c){
        return c.type_ == t;
    });
    if (it2==c_list.end()) {
        std::cerr << "get currency did not find ledger currency" << std::endl;
        return std::vector<currency>::iterator(nullptr);
    }
    return it2;
}

// ----------------------------------------------------------------------------
void xrpl_network::update_XRP_balance(std::string_view addr, double oldb, double newb)
{
    auto it = get_currency(addr, currency_type::xrp);
    if (it==std::vector<currency>::iterator(nullptr)) return;
    //
    if (it->balance_ != oldb) {
        std::cerr << "Old balance error " << it->balance_ << " expected " << oldb << std::endl;
    }
    std::cerr << "Balance updated from " << oldb << " to " << newb << std::endl;
    it->balance_ = newb;
    it->avail_   = newb - it->reserved_;

    // signal GUI to update
    emit update_currency_widget(&(*it));
}

// ----------------------------------------------------------------------------
// an IOU update sets the new balance directly - it does not add/subtract
void xrpl_network::update_IOU_balance(std::string_view addr, const currency &curr)
{
    auto it = get_currency(addr, curr.type_);
    if (it==std::vector<currency>::iterator(nullptr)) return;
    //
    double oldb = it->balance_;
    double newb = curr.balance_;
    std::cerr << "Balance updated from " << oldb << " to " << newb << std::endl;
    it->balance_ = newb;
    it->avail_   = newb - it->reserved_;

    // signal GUI to update
    emit update_currency_widget(&(*it));
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_balances()
{
    for (auto &w : subscribed_wallets_) {
        auto thread_function = [&]() {
            OB::Belle::Client new_client(dataapi_address(), dataapi_port(), true);
            // set the http 'on error' callback
            new_client.on_http_error([](auto& ctx) {
              std::cerr << "get_all_account_balances : Error: " << ctx.ec.message() << "\n\n";
            });

            std::string target = "/v2/accounts/" + w.public_ + "/balances";

            new_client.on_http(target, [this, &w](auto& ctx)
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
            new_client.connect();
        };
        auto https_thread = std::thread(std::move(thread_function));
        https_thread.detach();
    };
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
            currency c{"USD", currency::bitstamp_trust, currency_type::usd_bitstamp, b.value, b.value, 0, nullptr};
            add_currency(c, w.currencies_);
        }
        else if (b.currency == currency_type::eur_bitstamp) {
            currency c{"EUR", currency::bitstamp_trust, currency_type::eur_bitstamp, b.value, b.value, 0, nullptr};
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
    for (auto &w : subscribed_wallets_) {
        auto thread_function = [&]() {
            OB::Belle::Client new_client(jsonrpc_address(), jsonrpc_port(), true);
            // set the http 'on error' callback
            new_client.on_http_error([](auto& ctx) {
              std::cerr << "get_all_account_infos : Protocol Error: " << ctx.ec.message() << "\n\n";
            });

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

            new_client.on_http(req, [this, &w](auto& ctx)
            {
              if (ctx.res.result() != OB::Belle::Status::ok)
              {
                std::cerr << "account_info : " << w.public_ << " : HTTPS Error: " << ctx.res.result_int()
                          << " " << ctx.res.reason()
                          << "\n";
                return;
              }
              // debug : print the response headers and body
              DEBUG_ONLY(line_string << "account_info response : " << w.public_ << " : " << ctx.res.body());
              this->handle_account_info(w, std::move(ctx.res.body()));
            });
            new_client.connect();
        };
        auto https_thread = std::thread(std::move(thread_function));
        https_thread.detach();
    };
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
    std::string signed_tx;
    // are we sending xrp or an IOU?
    if (c.type_ == currency_type::xrp) {
        std::cout << "XRP payment amount " << c.balance_
                  << " from " << from->public_
                  << " to " << to->public_
                  << ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : "") << std::endl;

        signed_tx = make_xrp_payment(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                to->get_receive_address(c).begin(),
                to->tag_,
                static_cast<uint64_t>(c.balance_*1000000), "", "");
    }
    else {
        std::cout << "XRP IOU payment amount " << c.balance_
                  << " " << c.name_
                  << " from " << from->public_
                  << " to " << to->public_
                  << " IOU addr " << to->get_receive_address(c)
                  << ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : "") << std::endl;

        signed_tx = make_xrp_payment(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                to->get_receive_address(c).begin(),
                to->tag_,
                static_cast<uint64_t>(c.balance_*100), c.name_, c.issuer_);
    }
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


    auto thread_function = [&, req=std::move(req)]() {
        OB::Belle::Client new_client(jsonrpc_address(), jsonrpc_port(), true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "make_payment : Error: " << ctx.ec.message() << "\n\n";
        });

        new_client.on_http(req, [this](auto& ctx)
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
          emit transaction_event();
        });

        new_client.connect();
    };
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();

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
// place a buy/sell order
void xrpl_network::place_buy_limit_order(trade_data const &t)
{
    DEBUG_ALWAYS("Implement xrpl_network::place_buy_limit_order")
}

// ----------------------------------------------------------------------------
void xrpl_network::place_buy_sell_orders(std::vector<trade_data> const &trades)
{
    DEBUG_ALWAYS("Implement xrpl_network::place_buy_sell_orders")
}
