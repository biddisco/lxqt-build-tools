// STL
#include <string>
// Qt
#include <QObject>
#include <QString>
//
#include "src/debug.hpp"
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
//
#include "src/order_book.hpp"
#include "src/settings.hpp"
//
#include "src/widgets/currency_widget.hpp"
#include "src/widgets/xrp_functions.hpp"
//
#include "src/exchange/xrpl_network.hpp"
#include "src/exchange/bitstamp.hpp"
#include "src/exchange/xrpl.hpp"
// extern
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/Sign.h>
#include <ripple/protocol/UintTypes.h>
//
#include <QDialog>
#include <QHBoxLayout>

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
std::string xrpl_network::websocket_address() const {
    if (testnet_) return testnet_websocket_address;
    return ripple_websocket_address;
}
int xrpl_network::websocket_port() const {
    if (testnet_) return testnet_websocket_port;
    return ripple_websocket_port;
}

std::string xrpl_network::jsonrpc_address() const {
    if (testnet_) return testnet_json_rpc_address;
    return ripple_jsonrpc_address;
}
int xrpl_network::jsonrpc_port() const {
    if (testnet_) return testnet_json_rpc_port;
    return ripple_jsonrpc_port;
}

// ----------------------------------------------------------------------------
bool xrpl_network::can_send(currency &c, exchange *dest) {
    // yes to anything if the source is also an xrpl wallet
    if (dynamic_cast<xrpl_network*>(dest)) {
        return dynamic_cast<xrpl_network*>(dest)->testnet()==testnet();
    }
    else if (!testnet() && dynamic_cast<bitstamp_network*>(dest)) {
        if (c.type_==currency_type::xrp ||
                c.type_==currency_type::usd_bitstamp ||
                c.type_==currency_type::eur_bitstamp ||
                c.type_==currency_type::xrpl_trustline)
        {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
exchange::currency_pairlist xrpl_network::currency_pairs()
{
    currency c1 = currency{{currency::bitstamp_trust, "USD"}, currency_type::usd_bitstamp, 0, 0, 0, nullptr};
    currency c2 = currency{{"", "XRP"}, currency_type::xrp, 0, 0, 0, nullptr};
    currency c3 = currency{{currency::bitstamp_trust, "EUR"}, currency_type::eur_bitstamp, 0, 0, 0, nullptr};
    currency_pairlist supported = {{c1,c2}, {c2,c1}, {c3,c2}, {c2,c3}};
    return supported;
}

// ----------------------------------------------------------------------------
const xrpl_order_book &xrpl_network::get_orderbook() const
{
    return *orderbook_;
}

// ----------------------------------------------------------------------------
bool xrpl_network::connect(net::contexts &io_contexts, streams_vector const &streams)
{
    bool ok = true;
    for (const auto &s : streams) {
        if (s == network::streams::order_book) ok &= subscribe_orderbook(io_contexts);
        if (s == network::streams::accounts) ok &= subscribe_accounts(io_contexts);
    }
    return ok;
}

// ----------------------------------------------------------------------------
bool xrpl_network::disconnect(net::contexts &/*io_contexts*/, streams_vector const &streams)
{
    bool ok = true;
    for (const auto &s : streams) {
        if (s == network::streams::order_book) ws_orderbook->shutdown_blocking();
        if (s == network::streams::accounts) ws_accounts->shutdown_blocking();
    }
    return ok;
}

// ----------------------------------------------------------------------------
void xrpl_network::shut_down()
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
bool xrpl_network::subscribe_orderbook(net::contexts &io_contexts)
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
      websocket_address(), std::to_string(websocket_port()), subscription,
      std::bind(xrpl_network::new_orderbook_data, this, _1));

    return true;
}

// ----------------------------------------------------------------------------
bool xrpl_network::subscribe_accounts(net::contexts &io_contexts)
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
      websocket_address(), std::to_string(websocket_port()), subscription,
      std::bind(xrpl_network::new_account_data, this, _1));

    return true;
}

// ----------------------------------------------------------------------------
void xrpl_network::new_orderbook_data(xrpl_network* nw, std::string_view data)
{
    DEBUG_ONLY("xrpl new_orderbook_data : thread " << std::this_thread::get_id());

    if (startswith(data, "{\"result\":")) {
        DEBUG_ONLY("accept_json_ledger_snapshot");
        nw->orderbook_->accept_json_ledger_snapshot(data);
    }
    else if (startswith(data, "{\"engine_result\":")) {
        DEBUG_ONLY("accept_json_ledger_transaction");
        nw->orderbook_->accept_json_ledger_transaction(data);
    }

    // Send signal to mainwindow
    emit nw->orderbook_changed();
}

// ----------------------------------------------------------------------------
void xrpl_network::new_account_data(xrpl_network* nw, std::string_view data)
{
    DEBUG_ONLY("xrpl account_data : thread " << std::this_thread::get_id());
    DEBUG_ALWAYS("Account changes : " << data);
    if (startswith(data, "{\"result\":")) {
        // ignore this, just a subscription ok
        DEBUG_ONLY("Account subscription : " << data);
    }
    else if (startswith(data, "{\"engine_result\":\"tesSUCCESS\"")) {
        nlohmann::json jdata = json::parse(data);
        DEBUG_ONLY("Account changes : " << jdata.dump(4));
        nlohmann::json adata = jdata["meta"]["AffectedNodes"];
        for (const auto &a : adata) {
//            try {
            DEBUG_ALWAYS("AffectedNode : " << jdata.dump(4));
                if (a.contains("ModifiedNode")) {
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
                        curr.curr_.code_    = b["currency"].get<std::string>();
                        curr.curr_.issuer_  = t["SendMax"]["issuer"].get<std::string>();
                        curr.balance_       = std::stod(b["value"].get<std::string>());
                        curr.type_          = get_currency_type(curr.curr_);
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
                if (a.contains("DeletedNode")) {
                    auto t = jdata["transaction"];
                    if (t["TransactionType"].get<std::string>() == "OfferCancel") {
                        std::string  addr = t["Account"];
                        std::uint64_t seq = t["OfferSequence"];
                        auto it = nw->get_wallet_by_addr(addr);
                        if (!it) {
                            std::cerr << "Deleted node did not find wallet " << addr << std::endl;
                            break;
                        }
                        it->delete_trade(seq);
                        emit nw->transaction_event();
                    }
                }
//            }
//            catch (...) {
//                DEBUG_ALWAYS("Exception Account changes : " << data);
//            }
        }
    }
}

// ----------------------------------------------------------------------------
ledger_wallet *xrpl_network::get_wallet_by_addr(std::string_view addr)
{
    auto it = ranges::find_if(subscribed_wallets_, [addr](ledger_wallet const &w){
        return w.public_ == addr;
    });
    if (it==subscribed_wallets_.end()) {
        return nullptr;
    }
    return &(*it);
}

// ----------------------------------------------------------------------------
ledger_wallet *xrpl_network::get_wallet_by_name(std::string_view name)
{
    auto it = ranges::find_if(subscribed_wallets_, [name](ledger_wallet const &w){
        return w.name_ == name;
    });
    if (it==subscribed_wallets_.end()) {
        return nullptr;
    }
    return &(*it);
}

// ----------------------------------------------------------------------------
std::vector<currency>::iterator xrpl_network::get_currency(std::string_view addr, currency_type t)
{
    auto it = get_wallet_by_addr(addr);
    if (!it) {
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
OB::Belle::Request setup_request(const std::string &host, nlohmann::json &content)
{
    using namespace OB;
    Belle::Request req;
    // method
    req.method(Belle::Method::post);
    // headers
    req.set(Belle::Header::host, host);
    req.set(Belle::Header::user_agent, "mystery");
    req.set(Belle::Header::content_type, "application/json");
    req.set(Belle::Header::accept, "application/json");
    req.set(Belle::Header::connection, "close");
    // target path
    req.target("/");
    // contents
    req.body() = content.dump();
    // finalize
    req.prepare_payload();
    DEBUG_ONLY(line_string << req);
    //
    return req;
}

// ----------------------------------------------------------------------------
void xrpl_network::get_account_lines(std::string addr, fn_on_http on_http)
{
    auto thread_function = [=]() {
        OB::Belle::Client new_client(jsonrpc_address(), jsonrpc_port(), true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "get_account_balances : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        nlohmann::json params;
        params["account"] = addr;
        params["validated"] = true;

        nlohmann::json content;
        content["method"] = "account_lines";
        content["params"] = nlohmann::json::array({params});

        // init an http request object
        OB::Belle::Request req = setup_request(jsonrpc_address(), content);
        new_client.on_http(req, on_http);
        new_client.connect();
    };
    DEBUG_ONLY(addr + " Creating thread get_account_balances")
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_lines()
{
    for (auto &w : subscribed_wallets_) {
        get_account_lines(w.public_, [this, &w](auto& ctx)
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
              this->handle_account_lines(w, std::move(ctx.res.body()));
            }
        );
    };
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_lines(ledger_wallet &w, std::string&& data)
{
    nlohmann::json jdata = json::parse(data)["result"]["lines"];
    if (jdata.size()==0) {
        // no balances. Might be an inactive account
        return;
    }
    DEBUG_ONLY(jdata.dump(4));
    std::vector<xrp_amount> balances = jdata.get<std::vector<xrp_amount>>();
    //
    for (const auto &b : balances) {
        // @TODO, do not hardcode USD
        if (b.currency == currency_type::usd_bitstamp) {
            currency c{{currency::bitstamp_trust, "USD"}, currency_type::usd_bitstamp, b.value, b.value, 0, nullptr};
            w.add_currency(c);
        }
        else if (b.currency == currency_type::eur_bitstamp) {
            currency c{{currency::bitstamp_trust, "EUR"}, currency_type::eur_bitstamp, b.value, b.value, 0, nullptr};
            w.add_currency(c);
        }
        else if (b.currency == currency_type::xrp) {
            currency c{{"", "XRP"}, currency_type::xrp, b.value, b.value, 0, nullptr};
            w.add_currency(c);
        }
        else if (b.currency == currency_type::xrpl_trustline) {
            currency c{b.trustline.value(), currency_type::xrpl_trustline, b.value, b.value, 0, nullptr};
            w.add_currency(c);
            query_iou_fee(b.trustline.value());
        }
        else {
            throw std::runtime_error("Unknown currency in handle_account_balance");
        }
    }
    w.compute_ledger_reserve();

    // signal GUI to update
    emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
void xrpl_network::get_account_info(std::string addr, fn_on_http on_http)
{
    auto thread_function = [=]() {
        OB::Belle::Client new_client(jsonrpc_address(), jsonrpc_port(), true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "get_account_info : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        nlohmann::json params;
        params["account"] = addr;
        params["ledger_index"] = "current";
        params["strict"] = true;
        params["queue"] = true;

        nlohmann::json content;
        content["method"] = "account_info";
        content["params"] = nlohmann::json::array({params});

        // init an http request object
        OB::Belle::Request req = setup_request(jsonrpc_address(), content);
        new_client.on_http(req, on_http);
        new_client.connect();
    };
    DEBUG_ONLY(addr + " Creating thread get_account_info")
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}


// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_infos()
{
    for (auto &w : subscribed_wallets_) {
        fn_on_http func = [this, &w](auto& ctx) {
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
        };

        get_account_info(w.public_, func);
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
    //
    std::string bal = jdata.at("Balance").get< std::string >();
    double balance = std::stod(bal)/ 1E6;
    double avail = balance;
    double reserved = 0;
    //
    currency c{
        {"","XRP"},
        currency_type::xrp,
        balance,
        avail,
        reserved,
        nullptr
    };
    w.add_currency(c);
    w.compute_ledger_reserve();
    //
    DEBUG_ONLY(w.public_ << " Sequence " << w.sequence_);
    // signal GUI to update
    emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
void xrpl_network::get_account_offers(std::string addr, fn_on_http on_http)
{
    auto thread_function = [=]() {
        OB::Belle::Client new_client(jsonrpc_address(), jsonrpc_port(), true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "get_account_offers : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        nlohmann::json params;
        params["account"] = addr;

        nlohmann::json content;
        content["method"] = "account_offers";
        content["params"] = nlohmann::json::array({params});

        // init an http request object
        OB::Belle::Request req = setup_request(jsonrpc_address(), content);
        new_client.on_http(req, on_http);
        new_client.connect();
    };
    DEBUG_ONLY(addr + " Creating thread get_account_offers")
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}

// ----------------------------------------------------------------------------
void xrpl_network::get_all_account_offers()
{
    for (auto &w : subscribed_wallets_) {
        fn_on_http func = [this, &w](auto& ctx) {
            if (ctx.res.result() != OB::Belle::Status::ok)
            {
                std::cerr << "account_offers : " << w.public_ << " : HTTPS Error: " << ctx.res.result_int()
                          << " " << ctx.res.reason()
                          << "\n";
                return;
            }
            // debug : print the response headers and body
            DEBUG_ONLY(line_string << "account_offers response : " << w.public_ << " : " << ctx.res.body());
            this->handle_account_offers(w, std::move(ctx.res.body()));
        };

        get_account_offers(w.public_, func);
    };
}

// ----------------------------------------------------------------------------
void xrpl_network::handle_account_offers(ledger_wallet &w, std::string&& data)
{
    nlohmann::json jdata = json::parse(data)["result"];
    DEBUG_ONLY(jdata.dump(4));
    //
    if (jdata.is_null()) return;
    assert(w.public_ == jdata.at("account").get< std::string >());
    auto offers = jdata["offers"];
    if (offers.size()==0) return;
    //
    w.offers_.clear();
    for (const auto & offer : offers) {
        //
        xrp_amount taker_get;
        xrp_amount taker_pay;
        from_json(offer["taker_gets"], taker_get);
        from_json(offer["taker_pays"], taker_pay);
         //
        trade_data t{
            get_instance(testnet()),
            w.name_,
            taker_pay.currency,
            taker_get.currency,
            taker_pay.value,
            taker_get.value,
            0.0, // fee %
            0.0, // fee fixed
            0,   // id
            offer.at("seq").get<std::uint64_t>(),
            "- no date -"
        };

        // xrp amounts are in drops, do divide by 1E6 to get whole xrp units
        if (t.get_trade_type() == trade_type::buy) {
            t.taker_pay_ /= 1E6;
            t.exchange_rate_ = t.taker_get_ / t.taker_pay_;
        }
        else if (t.get_trade_type() == trade_type::sell) {
            t.taker_get_ /= 1E6;
            t.exchange_rate_ = t.taker_pay_ / t.taker_get_;
        }
        //
        w.offers_.push_back(t);
    }
    // signal GUI to update
    emit update_wallet_widget(&w);
}

// ----------------------------------------------------------------------------
bool xrpl_network::make_payment(currency &c, basic_account *src, basic_account *dest)
{
    ledger_wallet *from = static_cast<ledger_wallet*>(src);
    ledger_wallet *to = static_cast<ledger_wallet*>(dest);
    std::string signed_tx;
    // are we sending xrp or an IOU? xrp is always sent in drops
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
                c.balance_*1000000, "", "", 0.0);
    }
    else {
        double fee = get_transfer_fee(c);
        std::cout << "XRP IOU payment amount " << c.balance_
                  << " " << c.curr_.code_
                  << " TransferRate " << fee
                  << " from " << from->public_
                  << " to " << to->public_
                  << " IOU addr " << c.curr_.issuer_
                  << ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : "") << std::endl;

        signed_tx = make_xrp_payment(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                to->get_receive_address(c).begin(),
                to->tag_,
                c.balance_, c.curr_.code_, c.curr_.issuer_, fee);
    }
    from->sequence_++;
    submit_signed_transaction(std::move(signed_tx));
    return true;
}

// ----------------------------------------------------------------------------
void xrpl_network::submit_signed_transaction(std::string &&signed_tx)
{
    nlohmann::json tx;
    tx["tx_blob"] = signed_tx;

    nlohmann::json content;
    content["method"] = "submit";
    content["params"] = nlohmann::json::array({tx});

    // init an http request object
    OB::Belle::Request req = setup_request(jsonrpc_address(), content);

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
}

// ----------------------------------------------------------------------------
// place a buy/sell order
void xrpl_network::place_limit_order(basic_account *acct, trade_data const &t, bool update_after)
{
    using namespace ripple;
    //
    ledger_wallet *from = static_cast<ledger_wallet*>(acct);

    auto pay_pair = to_string(t.taker_payc_);
    auto get_pair = to_string(t.taker_getc_);
    ripple::STAmount taker_pays, taker_gets;

    // fiat currencies are multipled by 100 and shifted left by 2
    Currency curr_p = to_currency(pay_pair.first);
    if (is_fiat(t.taker_payc_)) {
        auto const issuer = parseBase58<AccountID>(pay_pair.second);
        taker_pays = STAmount(Issue(curr_p, *issuer), static_cast<uint64_t>(1E2*t.taker_pay_), -2);
    }
    // non fiat IOUs are multiplied by 1E6 and shifted right by 6 places
    else if (!is_xrp(t.taker_payc_)) {
        auto const issuer = parseBase58<AccountID>(pay_pair.second);
        taker_pays = STAmount(Issue(curr_p, *issuer), static_cast<uint64_t>(1E6*t.taker_pay_), -6);
    }
    // xrp is converted to drops by mutiplying by 1E6
    else if (is_xrp(t.taker_payc_)) {
        taker_pays = STAmount(XRPAmount(1E6*t.taker_pay_)); // drops
    }
    else {
        throw std::runtime_error("Unknown currency type taker_pays");
    }

    // fiat currencies are multipled by 100 and shifted left by 2
    Currency curr_g = to_currency(get_pair.first);
    if (is_fiat(t.taker_getc_)) {
        auto const issuer = parseBase58<AccountID>(get_pair.second);
        taker_gets = STAmount(Issue(curr_g, *issuer), static_cast<uint64_t>(1E2*t.taker_get_), -2);
    }
    // non fiat IOUs are multiplied by 1E6 and shifted right by 6 places
    if (!is_xrp(t.taker_getc_)) {
        auto const issuer = parseBase58<AccountID>(get_pair.second);
        taker_gets = STAmount(Issue(curr_g, *issuer), static_cast<uint64_t>(1E6*t.taker_get_), -6);
    }
    else if (is_xrp(t.taker_getc_)) {
        taker_gets = STAmount(XRPAmount(1E6*t.taker_get_)); // drops
    }
    else {
        throw std::runtime_error("Unknown currency type taker_gets");
    }

    // sign the transaction
    std::string signed_tx = make_xrp_offer(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                taker_pays,
                taker_gets,
                0);
    from->sequence_++;
    submit_signed_transaction(std::move(signed_tx));
}

// ----------------------------------------------------------------------------
void xrpl_network::place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades)
{
    for (auto const &t : trades) {
        //last order in list triggers update
        if (&t == &trades.back()) place_limit_order(acct, t, true);
        else place_limit_order(acct, t, false);
    }
}

// ----------------------------------------------------------------------------
void xrpl_network::cancel_order(trade_data const &t)
{
    ledger_wallet *from = get_wallet_by_name(t.wallet_);
    if (!from) {
        throw std::runtime_error("Cannot cancel order from " + t.wallet_);
    }

    // sign the transaction
    std::string signed_tx = cancel_xrp_offer(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                t.id_,
                0);
    from->sequence_++;
    submit_signed_transaction(std::move(signed_tx));
}

// ----------------------------------------------------------------------------
void xrpl_network::query_iou_fee(const issued_currency &c1)
{
    if (currency_fees_.find(c1.issuer_)!=currency_fees_.end()) {
        // no need to fetch it twice
        return;
    }

    fn_on_http func = [this,c1](auto& ctx) {
        if (ctx.res.result() != OB::Belle::Status::ok)
        {
            std::cerr << "account_info : " << c1.issuer_ << " : HTTPS Error: " << ctx.res.result_int()
                      << " " << ctx.res.reason()
                      << "\n";
            return;
        }
        // debug : print the response headers and body
        DEBUG_ONLY(line_string << "account_info response : " << c1.issuer_ << " : " << ctx.res.body());
        nlohmann::json jdata = json::parse(ctx.res.body())["result"]["account_data"];
        if (jdata.contains("TransferRate")) {
            int sfee = jdata["TransferRate"].get<int>();
            // In the XRP Ledger protocol, the transfer fee is specified in the TransferRate
            // field, as an integer which represents the amount you must send for the
            // recipient to get 1 billion units of the same currency.
            // A TransferRate of 1005000000 is equivalent to a transfer fee of 0.5%
            double feepercent = 100.0*(1E-9*sfee - 1.0);
            DEBUG_ALWAYS("fee % : " << c1.issuer_ << " : " << feepercent);
            currency_fees_[c1.issuer_] = feepercent;
        }
    };

    get_account_info(c1.issuer_, func);
}

// ----------------------------------------------------------------------------
double xrpl_network::get_transfer_fee(const currency &c1)
{
    return currency_fees_[c1.curr_.issuer_];
}

// ----------------------------------------------------------------------------
double xrpl_network::get_fee_percent(const currency_type &c1, const currency_type &c2)
{

    return 0.0;
}

// ----------------------------------------------------------------------------
double xrpl_network::get_fee_fixed(const currency_type &c1, const currency_type &c2)
{
    return 0.0;
}

// ----------------------------------------------------------------------------
void xrpl_network::trustline(basic_account *acct, std::string addr, std::string code, uint64_t limit, std::uint32_t flags)
{
    ledger_wallet *from = get_wallet_by_name(acct->name_);
    std::string signed_tx = set_trustline(
                ripple::KeyType::secp256k1,
                from->private_,
                from->public_,
                from->sequence_,
                limit, code, addr, flags);
    submit_signed_transaction(std::move(signed_tx));
}

// ----------------------------------------------------------------------------
void xrpl_network::custom_functions(basic_account *acct)
{
    std::cout << "Network function " << std::endl;
    QDialog dlg;
    xrp_functions *f = new xrp_functions(this, acct, &dlg);

    QHBoxLayout *HLayout = new QHBoxLayout(&dlg);
    HLayout->addWidget(f);
    dlg.setLayout (HLayout);
    dlg.exec();
}

// ----------------------------------------------------------------------------
//    {
//        using namespace std::chrono_literals;
//        using namespace jtx;
//        Basic_Con
//        Env env(*this);
//        env.fund(XRP(10000), "alice", "bob");
//        env.close();
//        auto wsc = makeWSClient(env.app().config());

//    }
