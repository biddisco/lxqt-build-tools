#include <QString>
#include <QMenu>
#include <QPlainTextEdit>
//
#include <string>
//
#include "src/print.hpp"
#include "src/settings.hpp"
//#include "src/util/stringutils.hpp"
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/exchange/xrpl_network.hpp"
#include "src/exchange/bitstamp.hpp"
#include "src/widgets/wallet_widget.hpp"
#include "src/util/stringutils.hpp"
#include "src/widgets/price_chart_widget.hpp"
//
#include "DockManager.h"
#include "DockWidget.h"
#include "DockAreaWidget.h"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 4;
//
template <int Level>
static print_threshold<Level, debug_level> bitstamp_dbg("Bitstamp");

// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
    // timer will fire once each time it is reset
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    bitstamp_account default_acct;
    default_acct.name_ = "Bitstamp Main";
    accounts_.push_back(default_acct);
    using namespace std::literals;
    token_expiry_ = std::chrono::steady_clock::now() - 60*1s;

    // used to fetch account balances after N seconds
    connect(timer_, SIGNAL(timeout()), this, SLOT(candlestick_timer_event()));
    // Timers must be started from the qt thread that created them
    connect(this, SIGNAL(restart_candlestick_timer()), this, SLOT(restart_candlestick_timer_event()));
    // after new data has been received, trigger this to process new candles and replot
    connect(this, SIGNAL(new_ohlc_data(ticker_data*, double)), this, SLOT(new_ohlc_data_event(ticker_data*, double)));

    connect(this, &bitstamp_network::new_trade_data_ui, this, [this](ticker_data tdata, live_trades t) {
        auto p = t.price;
        auto v = t.amount;
        QwtOHLCSample new_sample(1000.0*std::atof(t.timestamp.c_str()), p, p, p, p, v);
        tdata.view_->add_live_data(new_sample);
        tdata.chart_widget_->update_live_data(new_sample);
        // stream_process(new_sample);
    } , Qt::QueuedConnection);

}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network()
{
    bitstamp_dbg<0>.debug(str<>("destructor"));
    delete timer_;
    delete orderbook_;
}

// ----------------------------------------------------------------------------
void bitstamp_network::initialize()
{
    request_tickers_available();
    get_account_info();
    get_open_orders();
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_live_trades(const currency_pair &cp, net::contexts &io_contexts)
{
    using namespace std::placeholders;
    ticker_data tdata = tickers_subscribed_[cp];
    std::string ticker = currency_pair_lowercase_string(cp);
    ws_trades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        string_join("{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"live_trades_", ticker) + "\"}}",
        std::bind(bitstamp_network::new_trade_data, this, tdata, _1));

    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_order_book(const currency_pair &cp, net::contexts &io_contexts)
{
    using namespace std::placeholders;
    std::string ticker = currency_pair_lowercase_string(cp);
    bitstamp_dbg<0>.debug(str<>("Subscribing"), string_join("order_book_", ticker));
    ws_bidask = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        string_join("{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"order_book_", ticker) + "\"}}",
        std::bind(&bitstamp_network::new_orderbook_data, this, _1));

    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_trades(const currency_pair &cp, net::contexts &io_contexts)
{
    std::string ticker = currency_pair_lowercase_string(cp);
    nlohmann::json command;
    command["event"] = "bts:subscribe";
    command["data"]["channel"] = string_join("private-my_trades_", ticker) + "-" + websocket_user_id_;
    command["data"]["auth"] = websocket_token_;
    bitstamp_dbg<5>.debug(str<>("subscribe trades"), command.dump(4));

    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), string_join("my_trades_", ticker));
    ws_mytrades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        command.dump(4),
        [](std::string_view data) {
            bitstamp_dbg<0>.debug(str<>("Trades data"), data);
        });
    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::unsubscribe_my_trades(const currency_pair &cp)
{
    std::string ticker = currency_pair_lowercase_string(cp);
    nlohmann::json command;
    command["event"] = "bts:unsubscribe";
    command["data"]["channel"] = string_join("my_trades_", ticker) + "-" + get_bitstamp_instance()->account().API_user;
    command["data"]["auth"] = get_bitstamp_instance()->account().API_key;
    bitstamp_dbg<0>.debug(str<>("unsubscribe trades"), command.dump(4));
    ws_mytrades->write(command.dump(4));
    return true;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_my_orders(const currency_pair &cp, net::contexts &io_contexts)
{
    std::string ticker = currency_pair_lowercase_string(cp);
    nlohmann::json command;
    command["event"] = "bts:subscribe";
    command["data"]["channel"] = string_join("private-my_orders_", ticker) + "-" + websocket_user_id_;
    command["data"]["auth"] = websocket_token_;
    bitstamp_dbg<5>.debug(str<>("subscribe orders"), command.dump(4));

    using namespace std::placeholders;
    bitstamp_dbg<0>.debug(str<>("Subscribing"), string_join("my_orders_", ticker));
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
bool bitstamp_network::unsubscribe_my_orders(const currency_pair &cp)
{
    std::string ticker = currency_pair_lowercase_string(cp);
    nlohmann::json command;
    command["event"] = "bts:unsubscribe";
    command["data"]["channel"] = string_join("my_orders_", ticker) + "-" + get_bitstamp_instance()->account().API_user;
    command["data"]["auth"] = get_bitstamp_instance()->account().API_key;
    bitstamp_dbg<0>.debug(str<>("unsubscribe orders"), command.dump(4));
    ws_myorders->write(command.dump(4));
    return true;
}

// ----------------------------------------------------------------------------
// connect to (multiple) streams
bool bitstamp_network::websocket_connect(net::contexts &io_contexts, streams_vector const &streams)
{
    using namespace std::literals;
    auto now = std::chrono::steady_clock::now();
    while ((token_expiry_ - now)/1s < 5) {
        get_websocket_token();
        sleep(1);
    }
    //
    bool ok = true;
    currency_pair cp = string_to_pair("XRP-USD", "-");
    for (const auto &s : streams) {
        if (s == network::streams::my_trades)  ok &= subscribe_my_trades(cp, io_contexts);
        if (s == network::streams::my_orders)  ok &= subscribe_my_orders(cp, io_contexts);
        if (s == network::streams::trades)     ok &= subscribe_live_trades(cp, io_contexts);
        if (s == network::streams::order_book) ok &= subscribe_order_book(cp, io_contexts);
    }
    return ok;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::websocket_disconnect(net::contexts &/*io_contexts*/, streams_vector const &streams)
{
    currency_pair cp = string_to_pair("XRP", "USD");
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
    currency c1 = get_currency(p1);
    currency c2 = get_currency(p2);
    tickers_available_.push_back(std::make_pair(c1, c2));
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
    bitstamp_dbg<6>.debug(str<>("account info"), jdata.dump(4));
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
        bitstamp_dbg<7>.debug(str<>("Account request"), b_request);

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
          bitstamp_dbg<6>.debug(str<>("Request response"), ctx.res.body());
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
void bitstamp_network::new_trade_data(bitstamp_network* n, ticker_data tdata, std::string_view data)
{
    bitstamp_dbg<5>.debug(str<>("Trade data"), data);
    if (!startswith(data, "{\"data\":")) return;
    //
    nlohmann::json jdata = json::parse(data);
    // extract the main subgroup
    jdata = jdata["data"];
    bitstamp_dbg<7>.debug(str<>("Trade data parsed"), jdata.dump(4));

    live_trades trade_data = jdata.get<live_trades>();
    emit n->new_trade_data_ui(tdata, trade_data);
}

// ----------------------------------------------------------------------------
bool bitstamp_network::request_new_candlestick_data(std::string ticker, uint64_t start_t, fn_on_http_2 fn)
{
    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    uint64_t unixtime = currentDateTime.toSecsSinceEpoch();
    //
    uint64_t diff = unixtime - start_t;
    uint64_t samples = diff / 60;
    if (samples==0) {
        bitstamp_dbg<0>.debug(str<>("candlesticks"), ticker, "already up to date");
        return false;
    }
    //
    std::string req;
    bool repeat_ohlc = false;
    if (start_t == 0)
    {
        req = string_join("/api/v2/ohlc/", ticker) + "/?step=60&limit=1000";
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
        req = string_join("/api/v2/ohlc/", ticker) + "/?step=60&start=" + start + "&limit=" + limit;
        bitstamp_dbg<0>.debug(str<>("request"), ticker, req);
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
          bitstamp_dbg<6>.debug(str<>("Candlestick"), ctx.res.body());
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
          bitstamp_dbg<6>.debug(str<>("Tickers"), ctx.res.body());
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
        bitstamp_dbg<8>.debug(str<>("Currency pair"), c1, c2, c1, c2);
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
    bitstamp_dbg<0>.error(str<>("limit-order"), "@TODO USD assumption false");
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

// ----------------------------------------------------------------------------
void bitstamp_network::ticker_subscribe(const currency &c1, const currency &c2)
{
    // exit if this exchange has already subscribed to this ticker
    std::string cps = currency_pair_string({c1,c2});
    if (ticker_subscribed(c1,c2)) {
        bitstamp_dbg<0>.debug(str<>("subscription"), cps, "subscribed");
        return;
    }
    app_settings* app_ini = global_settings();
    bitstamp_dbg<0>.debug(str<>("subscribing"), cps);

    // create a new data view from hdf5
    std::shared_ptr<ohlc_dataset_view> view = app_ini->data_manager_->create_dataset_view("bitstamp", c1, c2);

    // create a new price plot object
    auto *chart_widget = new price_chart_widget(nullptr, view, shared_from_this(), cps);

    // add the subscribed ticker/data/plot to our list for tracking
    tickers_subscribed_.insert({currency_pair{c1,c2}, {view, chart_widget}});

    // put the price plot into a dock widget
    using namespace ads;
    std::string title = std::string(name()) + "-" + cps;
    CDockWidget* PlotDockWidget = new CDockWidget(QString(title.c_str()));
    PlotDockWidget->setWidget(chart_widget);
    PlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
    app_ini->dock_manager_->addDockWidget(DockWidgetArea::LeftDockWidgetArea, PlotDockWidget);
    app_ini->dockwindows_menu_->addAction(PlotDockWidget->toggleViewAction());

    // create a new orderbook text display
    const size_t font_size = 8;
    auto *orderbook_text = new QPlainTextEdit(nullptr);
    QString txt = "X";
    int char_size = QFontMetrics(orderbook_text->font()).horizontalAdvance(txt);
    int calcWidth = char_size*85 + 8;
    orderbook_text->setMinimumWidth(calcWidth);
    QFont font = QFont();
    font.setPointSize(font_size);
    font.setFamily("Courier");
    orderbook_text->setFont(font);

    // put the order book into a dock widget
    using namespace ads;
    std::string obtitle = "OrderBookText-" + std::string(name()) + "-" + cps;
    CDockWidget* obPlotDockWidget = new CDockWidget(QString(obtitle.c_str()));
    obPlotDockWidget->setWidget(orderbook_text);
    obPlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
    app_ini->dock_manager_->addDockWidget(DockWidgetArea::LeftDockWidgetArea, obPlotDockWidget);
    app_ini->dockwindows_menu_->addAction(obPlotDockWidget->toggleViewAction());

    // ----------------------------------
    // Create orderbook plot widget
    OrderBookPlot *orderbookplot = new OrderBookPlot();
    orderbookplot->setMinimumSize(384,256);
    set_plot(orderbookplot);
    //
    std::string obptitle = "OrderBookPlot-" + std::string(name()) + "-" + cps;
    CDockWidget* obpDockWidget = new CDockWidget(QString(obptitle.c_str()));
    obpDockWidget->setWidget(orderbookplot);
    obpDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
    app_ini->dock_manager_->addDockWidget(DockWidgetArea::CenterDockWidgetArea, obpDockWidget);
    app_ini->dockwindows_menu_->addAction(obpDockWidget->toggleViewAction());

    connect(this, &bitstamp_network::orderbook_changed, this, [orderbookplot, orderbook_text, this]() {
        QString datastring = QString::fromStdString(get_orderbook().order_text);
        orderbook_text->setPlainText(datastring);
        //
        orderbookplot->update_time_and_replot();
    }, Qt::QueuedConnection);

    // start by displaying 1 day of data
    chart_widget->graph_rescale(0);
}

// ----------------------------------------------------------------------------
void bitstamp_network::start_timer()
{
    timer_->start(2000);
}

// ----------------------------------------------------------------------------
void bitstamp_network::receive_ohlc_data(ticker_data *tdata, std::string&& data)
{
    try
    {
        // convert json data into vectors of actual data
        nlohmann::json jdata = json::parse(data)["data"]["ohlc"];
        bitstamp_dbg<0>.debug(str<>("received"), tdata->view_->get_ticker_string(), dec<4>(jdata.size()), "json OHLC samples");
        QVector<QwtOHLCSample> new_ohlc_samples;
        new_ohlc_samples.reserve(jdata.size());
        QwtOHLCSample sample;
        for (auto item : jdata) {
            sample.close  = atof(item["close"].get_ptr<json::string_t*>()->c_str());
            sample.high   = atof(item["high"].get_ptr<json::string_t*>()->c_str());
            sample.low    = atof(item["low"].get_ptr<json::string_t*>()->c_str());
            sample.open   = atof(item["open"].get_ptr<json::string_t*>()->c_str());
            sample.time   = atof(item["timestamp"].get_ptr<json::string_t*>()->c_str())*1000;
            sample.volume = atof(item["volume"].get_ptr<json::string_t*>()->c_str());
            new_ohlc_samples.push_back(sample);
        }
        //
        bitstamp_dbg<5>.debug(str<>("Converted"), tdata->view_->get_ticker_string(), new_ohlc_samples.size(), "new OHLC samples");
        tdata->view_->merge_data(ohlc_data_resolutions::minute, new_ohlc_samples);
        // what is the last sample we currently have
        auto last_time = tdata->view_->get_last_sample_time(false);
        bitstamp_dbg<0>.debug(str<>("data merged up to"), tdata->view_->get_ticker_string(), msecs_unix_to_calendar_time(last_time));
        tdata->view_->delete_live_data_up_to(last_time);
        //
        emit new_ohlc_data(tdata, ohlc_data_resolutions::minute);
    }
    catch (std::exception& e)
    {
        bitstamp_dbg<0>.error(str<>("JSON error"), "decoding OHLC data:", e.what(), "\n", data, "\n\n");
    }
}

// -------------------------------------------------10---------------------------
void bitstamp_network::new_ohlc_data_event(ticker_data *tdata, double old_res)
{
    (void)(old_res);
    bitstamp_dbg<5>.debug(str<>("new ohlc data"), "resolution", old_res);
    //
    // get all available candle resolutions, except highest res
    // since we we use that one to generate all the others
    const auto &resolutions = ohlc_data_resolutions::available_resolutions();
    for (size_t i=1; i<resolutions.size(); ++i) {
        auto const &res = resolutions[i];
        auto data = tdata->view_->get_dataset(res);
        if (data) {
            data->resample_update(res, tdata->view_->get_dataset(res.base_), ohlc_data_resolutions::get_resolution(res.base_));
        }
        else {
            data = tdata->view_->get_dataset(res.base_)->resample(res, ohlc_data_resolutions::get_resolution(res.base_));
            tdata->view_->add_dataset(res, data);
        }
    }

    // don't change axes, just update data series and replot
    tdata->chart_widget_->replot();
}

// ----------------------------------------------------------------------------
void bitstamp_network::update_ticker_data(currency_pair ticker, ticker_data *tdata)
{
    std::string ticker_lowercase = currency_pair_lowercase_string(ticker);
    std::string ticker_display   = currency_pair_string(ticker);
    bitstamp_dbg<6>.debug(str<>("update_ticker_data"), ticker_display);
    //
    //if (!candlestick_updates_active_.contains(ticker)) {
    // doesn't really matter if the key is already in the set
        candlestick_updates_active_.insert(ticker);
    //}

    uint64_t req_t = 0, start_t = 0;
    // what is the most recent sample we currently have
    start_t = static_cast<uint64_t>(tdata->view_->get_last_sample_time(false));
    if (start_t == 0) {
        // linux time 1496275200 = Thu Jun 01 2017 00:00:00 GMT+0000
        start_t = 1496275200*1000.0;
        std::string s = msecs_unix_to_calendar_time(start_t);
        bitstamp_dbg<5>.debug(str<>("No Data"), ticker_display, "requesting from", s);
    }
    else {
        std::string s = msecs_unix_to_calendar_time(start_t);
        bitstamp_dbg<5>.debug(str<>("data ok up to"), ticker_display, s);
    }
    // convert to unix timestamp : next sample is 60s after last
    req_t = start_t/1000 + 60;

    bitstamp_dbg<5>.debug(str<>("requesting"), ticker_display, msecs_unix_to_calendar_time(req_t*1000));

    // @TODO add futures here to make dependency chain simpler?
    bool ok = request_new_candlestick_data(ticker_lowercase, req_t, [this, req_t, ticker, tdata](auto& ctx, bool more) {
        bitstamp_dbg<0>.debug(str<>("received"), tdata->view_->get_ticker_string(), msecs_unix_to_calendar_time(req_t*1000));
        this->receive_ohlc_data(tdata, std::move(ctx.res.body()));
        if (more) {
            update_ticker_data(ticker, tdata);
        }
        else {
            candlestick_updates_active_.erase(ticker);
            emit restart_candlestick_timer();
        }
    });
    if (!ok) {
        // we were already up-to-date
        candlestick_updates_active_.erase(ticker);
        emit restart_candlestick_timer();
    }
}

// ----------------------------------------------------------------------------
void bitstamp_network::update_candlestick_data()
{
    for (auto &[ticker, data] : tickers_subscribed_)
    {
        if (!candlestick_updates_active_.contains(ticker)) {
            update_ticker_data(ticker, &data);
        }
    }
}

// ----------------------------------------------------------------------------
void bitstamp_network::candlestick_timer_event()
{
    QString now(QDateTime::currentDateTime().toString("dd.MM.yy hh:mm:ss"));
    bitstamp_dbg<5>.debug("candlestick_timer_event : " + now.toStdString());
    update_candlestick_data();
}

// ----------------------------------------------------------------------------
void bitstamp_network::restart_candlestick_timer_event()
{
    using namespace std::chrono;
    // how long until the minute candle closes
    // UTC! for local use # tm local_tm = *localtime(&tt);
    system_clock::time_point now = system_clock::now();
    time_t tt = system_clock::to_time_t(now);
    tm utc_tm = *gmtime(&tt);
    // we need to give bitstamp time to update its data,
    // so only check a few seconds after each new minute begins
    const int safety = 8;
    int delay_seconds = 60 + safety - utc_tm.tm_sec;

    if (timer_->isActive()) {
        // timer is already running
        bitstamp_dbg<5>.debug("Overriding: candlestick timer", delay_seconds, "seconds");
        timer_->start(delay_seconds*1000);
    }
    else {
        bitstamp_dbg<5>.debug("restarting candlestick timer", delay_seconds, "seconds");
        timer_->start(delay_seconds*1000);
    }
}
