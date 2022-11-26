#include <QString>
//
#include <string>
//
#include "src/print.hpp"
#include "src/exchange/xrpl_network.hpp"
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/widgets/wallet_widget.hpp"
//
#include "src/exchange/bitstamp.hpp"
#include "src/util/stringutils.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> bitstamp_dbg("Bitstamp");

// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
    bitstamp_account default_acct;
    default_acct.name_ = "Bitstamp Main";
    accounts_.push_back(default_acct);
    using namespace std::literals;
    token_expiry_ = std::chrono::steady_clock::now() - 60*1s;
}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network()
{
    bitstamp_dbg<0>.debug(str<>("destructor"));
    delete orderbook_;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_live_trades(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), "live_trades_xrpusd");
    ws_trades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"live_trades_xrpusd\"}}",
        std::bind(bitstamp_network::new_trade_data, this, _1));

    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_order_book(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), "order_book_xrpusd");
    ws_bidask = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"order_book_xrpusd\"}}",
        std::bind(&bitstamp_network::new_orderbook_data, this, _1));

    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_trades(net::contexts &io_contexts)
{
    nlohmann::json command;
    command["event"] = "bts:subscribe";
    command["data"]["channel"] = "private-my_trades_xrpusd-" + websocket_user_id_;
    command["data"]["auth"] = websocket_token_;
    bitstamp_dbg<5>.debug(str<>("subscribe trades"), command.dump(4));

    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), "my_trades_xrpusd");
    ws_mytrades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        command.dump(4),
        [](std::string_view data) {
            bitstamp_dbg<0>.debug(str<>("Trades data"), data);
        });
    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::unsubscribe_my_trades()
{
    nlohmann::json command;
    command["event"] = "bts:unsubscribe";
    command["data"]["channel"] = "my_trades_xrpusd-" + get_bitstamp_instance()->account().API_user;
    command["data"]["auth"] = get_bitstamp_instance()->account().API_key;
    bitstamp_dbg<0>.debug(str<>("unsubscribe trades"), command.dump(4));
    ws_mytrades->write(command.dump(4));
    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_orders(net::contexts &io_contexts)
{
    nlohmann::json command;
    command["event"] = "bts:subscribe";
    command["data"]["channel"] = "private-my_orders_xrpusd-" + websocket_user_id_;
    command["data"]["auth"] = websocket_token_;
    bitstamp_dbg<5>.debug(str<>("subscribe orders"), command.dump(4));

    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), "my_orders_xrpusd");
    ws_myorders = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        command.dump(4),
        [this](std::string_view data) {
            bitstamp_dbg<0>.debug(str<>("Orders data"), data);
            nlohmann::json jdata = json::parse(data);
            if (jdata["event"]=="bts:subscription_succeeded") {
                bitstamp_dbg<0>.debug(str<>("Orders data"), "bts:subscription_succeeded");
            }
            else {
                process_order(jdata["data"], jdata["event"].get<std::string_view>());
            }
        });
    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::unsubscribe_my_orders()
{
    nlohmann::json command;
    command["event"] = "bts:unsubscribe";
    command["data"]["channel"] = "my_orders_xrpusd-" + get_bitstamp_instance()->account().API_user;
    command["data"]["auth"] = get_bitstamp_instance()->account().API_key;
    bitstamp_dbg<0>.debug(str<>("unsubscribe orders"), command.dump(4));
    ws_myorders->write(command.dump(4));
    return true;
}

// ----------------------------------------------------------------------------
// connect to (multiple) streams
bool bitstamp_network::connect(net::contexts &io_contexts, streams_vector const &streams)
{
    using namespace std::literals;
    auto now = std::chrono::steady_clock::now();
    while ((token_expiry_ - now)/1s < 5) {
        get_websocket_token();
        sleep(1);
    }
    bool ok = true;
    for (const auto &s : streams) {
        if (s == network::streams::my_trades) ok &= subscribe_my_trades(io_contexts);
        if (s == network::streams::my_orders) ok &= subscribe_my_orders(io_contexts);
        if (s == network::streams::trades) ok &= subscribe_live_trades(io_contexts);
        if (s == network::streams::order_book) ok &= subscribe_order_book(io_contexts);
    }
    return ok;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::disconnect(net::contexts &/*io_contexts*/, streams_vector const &streams)
{
    bool ok = true;
    for (const auto &s : streams) {
        if (s == network::streams::my_trades) ws_mytrades->shutdown_blocking(); // unsubscribe_my_trades();
        if (s == network::streams::my_orders) ws_myorders->shutdown_blocking(); //unsubscribe_my_orders();
        if (s == network::streams::trades) ws_trades->shutdown_blocking();
        if (s == network::streams::order_book) ws_bidask->shutdown_blocking();
    }
    return ok;
}

// ----------------------------------------------------------------------------
void bitstamp_network::shut_down()
{
    bitstamp_dbg<0>.debug(str<>("websockets"), "shutdown start");
    if (ws_trades) {
        ws_trades->shutdown_blocking();
        ws_trades.reset();
    }
    if (ws_bidask) {
        ws_bidask->shutdown_blocking();
        ws_bidask.reset();
    }
}

// ----------------------------------------------------------------------------
bool bitstamp_network::add_currency_pair(std::string_view p1, std::string_view p2)
{
    auto icfn = [](std::string_view c) -> issued_currency {
        if (c=="USD" || c=="EUR" || c=="GBP")
            return issued_currency{currency::bitstamp_trust, std::string{c}};
        if (c=="XRP")
            return issued_currency{"", "XRP"};
        else
            return issued_currency{"", std::string{c}};
    };
    auto ic1 = icfn(p1);
    auto ic2 = icfn(p2);
    currency c1 = currency{ic1, get_currency_type(ic1), 0, 0, 0, nullptr};
    currency c2 = currency{ic2, get_currency_type(ic2), 0, 0, 0, nullptr};
    tickers_available_.push_back(std::make_pair(c1, c2));
    // tickers_available_.push_back(std::make_pair(c2, c1));
    return true;
}

// ----------------------------------------------------------------------------
const bitstamp_order_book &bitstamp_network::get_orderbook() const
{
    return *orderbook_;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::can_send(currency &c, exchange *dest)
{
    auto xrp_net = dynamic_cast<xrpl_network*>(dest);
    if (xrp_net && !xrp_net->testnet()) {
        if (c.type_==currency_type::xrp || c.type_==currency_type::usd_bitstamp || c.type_==currency_type::eur_bitstamp)
            return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
double bitstamp_network::get_fee_percent(const currency_type &c1, const currency_type &c2)
{
    // for bitstamp we put xrp first : "xrpusd_fee"
    std::pair<std::string, std::string> cpair;
    if (c1 == currency_type::xrp) {
        cpair = std::make_pair(
                lowercase(to_string(c1).first), lowercase(to_string(c2).first));
    }
    else {
        cpair = std::make_pair(
                lowercase(to_string(c2).first), lowercase(to_string(c1).first));
    }
    const auto val = fee_map_.at(cpair);
    return val;
}

// ----------------------------------------------------------------------------
double bitstamp_network::get_fee_fixed(const currency_type &c1, const currency_type &c2)
{
    return 0.0;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::make_payment(currency &c, basic_account *src, basic_account *dest)
{
    bitstamp_account *from = static_cast<bitstamp_account*>(src);
    ledger_wallet    *to = static_cast<ledger_wallet*>(dest);
    bitstamp_dbg<0>.debug(str<>("make_payment"), "amount", c.balance_
              , "currency", c.type_
              , "from",     from->name_
              , "to",       to->public_
              , ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : ""));

    std::stringstream req_string;
    req_string << "amount=" << c.balance_
               << "&address=" << to->public_;

    // issue a withdrawal payment to the wallet
    if (c.type_==currency_type::xrp) {
        req_string << "&destination_tag" << c.type_;
        //
        account_request("/api/v2/xrp_withdrawal/", req_string.str(), [](std::string &&data){
            bitstamp_dbg<0>.debug(str<>("request CB"), "/api/v2/xrp_withdrawal/", data);
        });
    }
    // this is an IOU transfer
    else {
        req_string << "&currency=" << c.type_;
        //
        account_request("/api/v2/ripple_withdrawal/", req_string.str(), [](std::string &&data){
            bitstamp_dbg<0>.debug(str<>("request CB"), "/api/v2/ripple_withdrawal/", data);
        });
    }

    emit transaction_event();
    return true;
}

// ----------------------------------------------------------------------------
void bitstamp_network::set_plot(OrderBookPlot *obp) {
    orderbook_ = new bitstamp_order_book(obp, false);
}

// ----------------------------------------------------------------------------
void bitstamp_network::get_account_info()
{
    account_request("/api/v2/balance/", "", [this](std::string &&data) {
        handle_account_info(std::move(data));
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::get_websocket_token()
{
    account_request("/api/v2/websockets_token/", "", [this](std::string &&data) {
        handle_websockets_token(std::move(data));
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_account_info(std::string&& data)
{
    nlohmann::json jdata = json::parse(data);
    bitstamp_dbg<5>.debug(str<>("account info"), jdata.dump(4));
    //
    bitstamp_account &acct = get_bitstamp_instance()->account();

    currency xrp_bitstamp{
        {"", "XRP"}, currency_type::xrp,
        std::stod(jdata["xrp_balance"].get_ptr<json::string_t*>()->c_str()),
        std::stod(jdata["xrp_available"].get_ptr<json::string_t*>()->c_str()),
        std::stod(jdata["xrp_reserved"].get_ptr<json::string_t*>()->c_str()),
        nullptr
    };
    acct.add_currency(xrp_bitstamp);

    if (jdata.contains("usd_balance")) {
        currency usd_bitstamp{
            {currency::bitstamp_trust, "USD"}, currency_type::usd_bitstamp,
            std::stod(jdata["usd_balance"].get_ptr<json::string_t*>()->c_str()),
            std::stod(jdata["usd_available"].get_ptr<json::string_t*>()->c_str()),
            std::stod(jdata["usd_reserved"].get_ptr<json::string_t*>()->c_str()),
            nullptr
        };
        acct.add_currency(usd_bitstamp);
    }

    if (jdata.contains("eur_balance")) {
        currency eur_bitstamp{
            {currency::bitstamp_trust, "EUR"}, currency_type::eur_bitstamp,
            std::stod(jdata["eur_balance"].get_ptr<json::string_t*>()->c_str()),
            std::stod(jdata["eur_available"].get_ptr<json::string_t*>()->c_str()),
            std::stod(jdata["eur_reserved"].get_ptr<json::string_t*>()->c_str()),
            nullptr
        };
        acct.add_currency(eur_bitstamp);
    }

    if (jdata.contains("xrpusd_fee")) {
        double xrpusd_fee = std::stod(jdata["xrpusd_fee"].get_ptr<json::string_t*>()->c_str());
        std::pair<std::string, std::string> cpair = std::make_pair("xrp", "usd");
        const auto [it, success] = fee_map_.insert({cpair, xrpusd_fee});
        if (success) {
            bitstamp_dbg<0>.debug(str<>("new fee xrp/usd"), xrpusd_fee);
        }
        else {
            bitstamp_dbg<0>.debug(str<>("replace fee xrp/usd"), xrpusd_fee);
            fee_map_[cpair] = xrpusd_fee;
        }
    }

    if (jdata.contains("xrpeur_fee")) {
        double xrpeur_fee = std::stod(jdata["xrpeur_fee"].get_ptr<json::string_t*>()->c_str());
        std::pair<std::string, std::string> cpair = std::make_pair("xrp", "eur");
        const auto [it, success] = fee_map_.insert({cpair, xrpeur_fee});
        if (success) {
            bitstamp_dbg<0>.debug(str<>("new fee xrp/eur"), xrpeur_fee);
        }
        else {
            bitstamp_dbg<0>.debug(str<>("replace fee xrp/eur"), xrpeur_fee);
            fee_map_[cpair] = xrpeur_fee;
        }
    }
    emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_websockets_token(std::string&& data)
{
    nlohmann::json jdata = json::parse(data);
    bitstamp_dbg<0>.debug(str<>("websocket token"), jdata.dump());
    //
    bitstamp_account &acct = get_bitstamp_instance()->account();
    //
    using namespace std::literals;
    auto valid_sec     = jdata["valid_sec"].get<int>();
    websocket_token_   = jdata["token"].get<std::string>();
    websocket_user_id_ = std::to_string(jdata["user_id"].get<int>());
    token_expiry_ = std::chrono::steady_clock::now() + valid_sec*1s;
}

// ----------------------------------------------------------------------------
void bitstamp_network::get_open_orders()
{
    account_request("/api/v2/open_orders/all/", "", [this](std::string &&data) {
        bitstamp_dbg<0>.debug(str<>("Open Order response"), data);
        handle_open_orders(std::move(data));
    });
}

// ----------------------------------------------------------------------------
/* A typical order will have the following structure

{
  "data": {
    "id": 1552110926278656,
    "id_str": "1552110926278656",
    "order_type": 1,
    "datetime": "1667768306",
    "microtimestamp": "1667768306268000",
    "amount": 1000,
    "amount_str": "1000.00000000",
    "price": 0.54,
    "price_str": "0.54000"
  },
  "channel": "private-my_orders_xrpusd-1227955",
  "event": "order_created"
}
{
  "data": {
    "id": 1552274932695043,
    "id_str": "1552274932695043",
    "order_type": 1,
    "datetime": "1667808347",
    "microtimestamp": "1667808346897000",
    "amount": 1000,
    "amount_str": "1000.00000000",
    "price": 0.54,
    "price_str": "0.54000"
  },
  "channel": "private-my_orders_xrpusd-1227955",
  "event": "order_created"
}

*/
void bitstamp_network::process_order(nlohmann::json &jdata, std::string_view event)
{
    bitstamp_account &acct = get_bitstamp_instance()->account();
    auto &trades = acct.offers_;
    //
    std::uint64_t id = jdata["id"];
    auto find_by_id = [id](trade_data &t){
        return t.id_ == id;
    };

    if (event == "order_deleted") {
        auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
        if (trade==trades.end()) {
            bitstamp_dbg<0>.error(str<>("Order not found"), dec<18>(id));
        }
        else {
            bitstamp_dbg<0>.debug(str<>("Order deleted"), dec<18>(id));
            trades.erase(trade);
        }
    }
    if (event == "order_created") {
        auto trade = std::find_if(trades.begin(), trades.end(), find_by_id);
        if (trade==trades.end()) {
            bitstamp_dbg<0>.error(str<>("Order not found"), dec<18>(id));
        }
        else {
            if (trade->confirmed_ == false) {
                bitstamp_dbg<0>.debug(str<>("Order created"), dec<18>(id), "confirmed");
                trade->confirmed_ = true;
            }
            else {
                bitstamp_dbg<0>.error(str<>("Order created"), dec<18>(id), "already active");
            }
        }
    }
    emit update_wallet_widget(&acct);
}

/* A list of open orders take the form
[
  {
    "id": "1552120149377025",
    "datetime": "2022-11-06 21:35:58",
    "type": "1",
    "amount": "50000.00000000",
    "price": "0.54200",
    "amount_at_create": "50000.00000000",
    "currency_pair": "XRP/USD"
  }
]

*/

void bitstamp_network::handle_open_orders(std::string&& data)
{
    bitstamp_account &acct = get_bitstamp_instance()->account();
    auto &trades = acct.offers_;
    trades.clear();
    //
    nlohmann::json jdata = json::parse(data);
    for (auto& [key, val] : jdata.items())
    {
        const auto & [c1, c2] = get_currency_pair(JCHARP(val["currency_pair"]));

        double amount = std::stod(JCHARP(val["amount"]));
        double price = std::stod(JCHARP(val["price"]));
        double fee_percent = 0;
        double fee_fixed = 0;
        // 0=buy, 1=sell
        auto trade_type_ = (val["type"] == "0") ? trade_type::buy : trade_type::sell;

        if (trade_type_ == trade_type::sell) {
            trade_data t{
                this->get_instance(),
                account().name_,
                get_currency_type({"", c2.cbegin()}),
                get_currency_type({"", c1.cbegin()}),
                amount*price,
                amount,
                price,
                fee_percent,
                fee_fixed,
                std::stoull(val["id"].get< std::string >()),
                val["datetime"],
                true
            };
            trades.push_back(t);
        }
        else {
            trade_data t{
                this->get_instance(),
                account().name_,
                get_currency_type({"", c1.cbegin()}),
                get_currency_type({"", c2.cbegin()}),
                amount,
                amount*price,
                price,
                fee_percent,
                fee_fixed,
                std::stoull(val["id"].get< std::string >()),
                val["datetime"],
                true
            };
            trades.push_back(t);
        }
    }
    emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::account_request(std::string &&url_path, std::string &&url_query, request_callback &&cb)
{
    // NB. HTTP POST request uses payload for parameters, not URI/URL
    auto thread_function = [cb=std::move(cb), url_path=std::move(url_path), url_query=std::move(url_query)]() {
        OB::Belle::Client new_client(bitstamp_https_address, bitstamp_https_port, true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "account_request : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
        encryption encryptor(get_bitstamp_instance()->account().API_key, randbytes);
        //
        std::chrono::milliseconds timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());
        // setup REST request fields
        std::string x_auth = "BITSTAMP " + get_bitstamp_instance()->account().API_key;
        std::string x_auth_nonce = encryptor.generate_uuid_string();
        std::string x_auth_timestamp = std::to_string(timestamp.count());
        std::string x_auth_version = "v2";
        std::string content_type = "application/x-www-form-urlencoded";
        std::string payload = url_query.size()>0 ? url_query : url_encode("{offset:1}");
        std::string http_method = "POST";

        // full query is signed using Hmac SHA256 algorithm
        std::string data_to_sign = "";
        data_to_sign.append(x_auth);
        data_to_sign.append(http_method);
        data_to_sign.append(bitstamp_https_address);
        data_to_sign.append(url_path);
        data_to_sign.append("");
        data_to_sign.append(content_type);
        data_to_sign.append(x_auth_nonce);
        data_to_sign.append(x_auth_timestamp);
        data_to_sign.append(x_auth_version);
        data_to_sign.append(payload);

        // generated signature
        auto signed_hmac = encryptor.CalcHmacSHA256(get_bitstamp_instance()->account().API_secret, data_to_sign);
        assert(signed_hmac.size() == 32);
        std::string x_auth_signature = b2a_hex(signed_hmac.data(), signed_hmac.size());

        OB::Belle::Request b_request;
        b_request = OB::Belle::Request(http::verb::post, url_path, 11);
        b_request.target(url_path);
        b_request.set(http::field::host, bitstamp_https_address);
        b_request.set(http::field::content_type, content_type);
        b_request.set("X-Auth", x_auth);
        b_request.set("X-Auth-Signature", x_auth_signature);
        b_request.set("X-Auth-Nonce", x_auth_nonce);
        b_request.set("X-Auth-Timestamp", x_auth_timestamp);
        b_request.set("X-Auth-Version", x_auth_version);
        //
        b_request.body() = payload;
        b_request.prepare_payload();
        bitstamp_dbg<5>.debug(str<>("Account request"), b_request);

        new_client.on_http(b_request, [cb=std::move(cb)](auto& ctx)
        {
          // check http status code
          if (ctx.res.result() != OB::Belle::Status::ok)
          {
            // print the response status code and reason
            std::cerr << "HTTPS Error: (belle_https_bitstamp): "
                      << ctx.res.result_int()
                      << " " << ctx.res.reason() << "\n\n";
            return;
          }
          // debug : print the response headers and body
          bitstamp_dbg<5>.debug(str<>("Request response"), ctx.res.body());
          cb(std::move(ctx.res.body()));
        });
        new_client.connect();
    };
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_orderbook_data(bitstamp_network* n, std::string_view data)
{
    bitstamp_dbg<5>.debug(str<>("Orderbook data"), data);

    if (!n->orderbook_->accept_json_bitstamp(data))
        return;

    emit n->orderbook_changed();
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_trade_data(bitstamp_network* n, std::string_view data)
{
    bitstamp_dbg<5>.debug(str<>("Trade data"), data);
    if (!startswith(data, "{\"data\":")) return;
    //
    nlohmann::json jdata = json::parse(data);
    // extract the main subgroup
    jdata = jdata["data"];
    bitstamp_dbg<5>.debug(str<>("Trade data parsed"), jdata.dump(4));

    live_trades trade_data = jdata.get<live_trades>();

    //    emit candlestickdata(ohlc_vector);

    emit n->new_trade_data_ui(trade_data);
}

// ----------------------------------------------------------------------------
bool bitstamp_network::request_new_candlestick_data(uint64_t start_t, fn_on_http_2 fn)
{
    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    uint64_t unixtime = currentDateTime.toTime_t();
    //
    uint64_t diff = unixtime - start_t;
    uint64_t samples = diff / 60;
    if (samples==0) {
        bitstamp_dbg<0>.debug(str<>("candlesticks"), "already up to date");
        return false;
    }
    //
    std::string req;
    bool repeat_ohlc = false;
    if (start_t == 0)
    {
        req = "/api/v2/ohlc/xrpusd/?step=60&limit=1000";
    }
    else
    {
        if (samples >= 1000)
        {
            bitstamp_dbg<5>.debug(str<>("Limiting request"), samples);
            samples = 1000;
            repeat_ohlc = true;
        }
        std::string start = std::to_string(start_t);
        std::string limit = std::to_string(samples);
        // send a request for ticker data using the io context thread to make the request
        req = "/api/v2/ohlc/xrpusd/?step=60&start=" + start + "&limit=" + limit;
        bitstamp_dbg<0>.debug(str<>("request"), req);
    }

    auto thread_function = [req=std::move(req), fn=std::move(fn), repeat_ohlc]() {
        OB::Belle::Client new_client(bitstamp_https_address, bitstamp_https_port, true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "account_request : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        new_client.on_http(req, [fn, repeat_ohlc](auto& ctx) mutable
        {
          // check http status code
          if (ctx.res.result() != OB::Belle::Status::ok)
          {
            // print the response status code and reason
            bitstamp_dbg<0>.error(str<>("HTTPS Error:")
                        , ctx.res.result_int()
                        , ctx.res.reason());
            return;
          }
          // debug : print the response headers and body
          bitstamp_dbg<5>.debug(str<>("Candlestick"), ctx.res.body());
          fn(ctx, repeat_ohlc);
        });
        new_client.connect();
    };
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
    return true;
}

// ----------------------------------------------------------------------------
void bitstamp_network::request_tickers_available()
{
    std::string req = "https://www.bitstamp.net/api/v2/ticker/";
    bitstamp_dbg<0>.debug(str<>("Tickers Request"), req);

    auto thread_function = [this, req=std::move(req)]() {
        OB::Belle::Client new_client(bitstamp_https_address, bitstamp_https_port, true);
        // set the http 'on error' callback
        new_client.on_http_error([](auto& ctx) {
          std::cerr << "Tickers Request : Protocol Error: " << ctx.ec.message() << "\n\n";
        });

        new_client.on_http(req, [this](auto& ctx) mutable
        {
          // check http status code
          if (ctx.res.result() != OB::Belle::Status::ok)
          {
            // print the response status code and reason
            bitstamp_dbg<0>.error(str<>("HTTPS Error:")
                        , ctx.res.result_int()
                        , ctx.res.reason());
            return;
          }
          // debug : print the response headers and body
          bitstamp_dbg<5>.debug(str<>("Tickers"), ctx.res.body());
          receive_tickers_available(std::move(ctx.res.body()));
        });
        new_client.connect();
    };
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}

// ----------------------------------------------------------------------------
void bitstamp_network::receive_tickers_available(std::string &&data)
{
    nlohmann::json jdata = json::parse(data);
    for (auto& [key, val] : jdata.items())
    {
        const auto & [c1, c2] = get_currency_pair(JCHARP(val["pair"]));
        bitstamp_dbg<5>.debug(str<>("Currency pair"), c1, c2, c1, c2);
        add_currency_pair(c1, c2);
    }

    emit network_initialized(this);
}

// ----------------------------------------------------------------------------
void bitstamp_network::cancel_order(trade_data const &t)
{
    std::string data = "&id=" + std::to_string(t.id_);
    account_request("/api/v2/cancel_order/", std::move(data), [this](std::string &&data) {
        nlohmann::json jdata = json::parse(data);
        bitstamp_dbg<0>.debug(str<>("Cancel Order response"), jdata.dump(4));
        // refresh order status
        if (ws_myorders==nullptr) {
            get_open_orders();
        }
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_limit_order(trade_data const &t, bool update_after)
{
    double amount = t.get_xrp_amount();

    // bitstamp trade pair is always xrpusd, so swap symbols accordingly
    std::string req, data;
    if (t.get_trade_type()==trade_type::buy) {
        req = std::string("/api/v2/buy/")
                + std::string(to_string(t.taker_payc_).first)
                + std::string(to_string(t.taker_getc_).first) + "/";
        data = "&amount=" + to_string(amount, t.taker_payc_)
                + "&price=" + to_string_with_precision(t.exchange_rate_, 5);
    }
    else {
        req = std::string("/api/v2/sell/")
                + std::string(to_string(t.taker_getc_).first)
                + std::string(to_string(t.taker_payc_).first) + "/";
        data = "&amount=" + to_string(amount, t.taker_getc_)
                + "&price=" + to_string_with_precision(t.exchange_rate_, 5);
    }
    // make lowercase "XRPUSD"->"xrpusd" for bitstamp API
    lowercase_i(req);
    //
    bitstamp_dbg<0>.debug(str<>("limit-order"), (t.get_trade_type()==trade_type::buy ? "Buy" : "Sell"), req, data);

    account_request(std::move(req), std::move(data), [this, t, update_after](std::string &&data) {
        nlohmann::json jdata = json::parse(data);
        bitstamp_dbg<0>.debug(str<>("limit-order response"), jdata.dump(4));
        // refresh order status if we don't have orders websocket
        if (update_after && ws_myorders==nullptr) {
            get_open_orders();
        }
        if (jdata.contains("id")) {
            trade_data new_t = t;
            new_t.id_ = std::stoll(jdata["id"].get_ptr<json::string_t*>()->c_str());
            new_t.confirmed_ = true;
            new_t.datetime_ = jdata["datetime"].get_ptr<json::string_t*>()->c_str();
            double price  = std::stod(jdata["price"].get_ptr<json::string_t*>()->c_str());
            double amount = std::stod(jdata["amount"].get_ptr<json::string_t*>()->c_str());
            if (price != new_t.get_price()) {
                bitstamp_dbg<0>.error(str<>("limit-order price"), price, new_t.get_price());
            }
            auto &trades = account().offers_;
            trades.push_back(new_t);
            emit update_wallet_widget(&account());
        }
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades)
{
    for (auto &t : trades) {
        if (&t != &trades.back()) place_limit_order(t, false);
        else place_limit_order(t, true);
    }
}
