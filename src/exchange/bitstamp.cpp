#include <QApplication>
#include <QString>
#include <QTimer>
//
#include <string>
//
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/wallet_widget.hpp"
//
#include "bitstamp.hpp"
//
// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network()
{
    qDebug() << "bitstamp_network: destructor";
    delete orderbook_;
}

// ----------------------------------------------------------------------------
void bitstamp_network::connect(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    ws_trades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"live_trades_xrpusd\"}}",
        std::bind(bitstamp_network::new_trade_data, this, _1));

    ws_bidask = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"order_book_xrpusd\"}}",
        std::bind(&bitstamp_network::new_orderbook_data, this, _1));
}

// ----------------------------------------------------------------------------
void bitstamp_network::disconnect()
{
    qDebug() << "bitstamp_network: websockets: shutdown start";

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
std::vector<std::pair<currency_type, currency_type>> bitstamp_network::currency_pairs()
{
    std::vector<std::pair<currency_type, currency_type>> supported = {
        {usd_bitstamp,xrp},
        {xrp,usd_bitstamp},
        {eur_bitstamp,xrp},
        {xrp,eur_bitstamp},
    };
    return supported;
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
bool bitstamp_network::make_payment(currency &c, basic_account *src, basic_account *dest)
{
    std::cout << "Bitstamp payment sent" << std::endl;

    bitstamp_account *from = static_cast<bitstamp_account*>(src);
    ledger_wallet    *to = static_cast<ledger_wallet*>(dest);
    std::cout << "Bitstamp payment amount " << c.balance_
              << " currency " << c.type_
              << " from " << from->name_
              << " to "   << to->public_
              << ((to->tag_!=0) ? "(" + std::to_string(to->tag_) + ")" : "") << std::endl;

    std::stringstream req_string;
    req_string << "amount=" << c.balance_
               << "&address=" << to->public_;

    // issue a withdrawal payment to the wallet
    if (c.type_==currency_type::xrp) {
        req_string << "&destination_tag" << c.type_;
        //
        account_request("/api/v2/xrp_withdrawal/", req_string.str(), [](std::string &&data){
            std::cout << "/api/v2/xrp_withdrawal/ " << data << std::endl;
        });
    }
    // this is an IOU transfer
    else {
        req_string << "&currency=" << c.type_;
        //
        account_request("/api/v2/ripple_withdrawal/", req_string.str(), [](std::string &&data){
            std::cout << "/api/v2/ripple_withdrawal/ " << data << std::endl;
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
void bitstamp_network::update_account_info()
{
    account_request("/api/v2/balance/", "", [this](std::string &&data) {
        account_data(std::move(data));
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::get_open_orders()
{
    account_request("/api/v2/open_orders/all/", "", [this](std::string &&data) {
        DEBUG_ONLY("Open Order response:\n" << data);
        QString sdata(data.c_str());
        emit user_trades_updated(sdata);
    });
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

        app_settings* app_ini = global_settings();
        secure_string randbytes = generate_random_alphanumeric_string(encryption::KEY_SIZE, 81192);
        encryption encryptor(app_ini->bitstamp.API_key, randbytes);
        //
        std::chrono::milliseconds timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());
        // setup REST request fields
        std::string x_auth = "BITSTAMP " + app_ini->bitstamp.API_key;
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
        auto signed_hmac = encryptor.CalcHmacSHA256(app_ini->bitstamp.API_secret, data_to_sign);
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
        DEBUG_ALWAYS("Request " << b_request << "\n");

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
          DEBUG_ONLY("Request response " << ctx.res.body() << "\n");
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
    DEBUG_ONLY("bitstamp orderbook_data : thread " << std::this_thread::get_id());
    DEBUG_ONLY("\n\nReceived " << data << std::endl << std::endl);

    if (!n->orderbook_->accept_json_bitstamp(data))
        return;

    emit n->orderbook_changed();
}

// ----------------------------------------------------------------------------
void bitstamp_network::account_data(std::string&& data)
{
    DEBUG_ONLY("bitstamp account_data : thread " << std::this_thread::get_id());
    DEBUG_ONLY("Response : " << data);
    //
    nlohmann::json jdata = json::parse(data);
    DEBUG_ONLY(jdata.dump(4));
    //
    app_settings* app_ini = global_settings();
    //
    currency xrp_bitstamp{
        "XRP", "", currency_type::xrp,
        std::stod(jdata["xrp_balance"].get<std::string>()),
        std::stod(jdata["xrp_available"].get<std::string>()),
        std::stod(jdata["xrp_reserved"].get<std::string>()),
        nullptr
    };
    add_currency(xrp_bitstamp, app_ini->bitstamp.currencies_);

    if (jdata.contains("usd_balance")) {
        currency usd_bitstamp{
            "USD", currency::bitstamp_trust, currency_type::usd_bitstamp,
            std::stod(jdata["usd_balance"].get<std::string>()),
            std::stod(jdata["usd_available"].get<std::string>()),
            std::stod(jdata["usd_reserved"].get<std::string>()),
            nullptr
        };
        add_currency(usd_bitstamp, app_ini->bitstamp.currencies_);
    }

    if (jdata.contains("eur_balance")) {
        currency eur_bitstamp{
            "EUR", currency::bitstamp_trust, currency_type::eur_bitstamp,
            std::stod(jdata["eur_balance"].get<std::string>()),
            std::stod(jdata["eur_available"].get<std::string>()),
            std::stod(jdata["eur_reserved"].get<std::string>()),
            nullptr
        };
        add_currency(eur_bitstamp, app_ini->bitstamp.currencies_);
    }

    emit widget_update();
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_trade_data(bitstamp_network* n, std::string_view data)
{
    DEBUG_ONLY("bitstamp trade_data : thread " << std::this_thread::get_id());
    DEBUG_ONLY("\n\nReceived " << data);
    if (!startswith(data, "{\"data\":")) return;
    //
    nlohmann::json jdata = json::parse(data);
    // extract the main subgroup
    jdata = jdata["data"];
    DEBUG_ONLY(jdata.dump(4));

    live_trades json_trades = jdata.get<live_trades>();

    // convert json data to trade structs
    //    live_trades_string json_strings;
    //    // convert json to vector of structs
    //    json_strings = jdata.get<live_trades_string>();
    //    live_trades json_trade(json_strings);

    //    emit candlestickdata(ohlc_vector);

    QString datastring = QString::fromStdString(jdata.dump(4));
    emit n->new_trade_data_ui(datastring);
}

// ----------------------------------------------------------------------------
void bitstamp_network::request_new_candlestick_data(uint64_t start_t, fn_on_http_2 fn)
{
    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    uint64_t unixtime = currentDateTime.toTime_t();
    //
    uint64_t diff = unixtime - start_t;
    uint64_t samples = diff / 60;
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
            DEBUG_ONLY("Limiting request from: " << samples);
            samples = 1000;
            repeat_ohlc = true;
        }
        std::string start = std::to_string(start_t);
        std::string limit = std::to_string(samples);
        // send a request for ticker data using the io context thread to make the request
        req = "/api/v2/ohlc/xrpusd/?step=60&start=" + start + "&limit=" + limit;
        std::cout << req << std::endl;
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
            std::cerr << "HTTPS Error: "
                      << ctx.res.result_int()
                      << " " << ctx.res.reason() << "\n\n";
            return;
          }
          // debug : print the response headers and body
          DEBUG_ONLY("Candlestick response " << ctx.res.body() << "\n");
          fn(ctx, repeat_ohlc);
        });
        new_client.connect();
    };
    auto https_thread = std::thread(std::move(thread_function));
    https_thread.detach();
}

// ----------------------------------------------------------------------------
void bitstamp_network::timer_event()
{
//    std::cout << "Timer event : requesting account update" << std::endl;
//    update_account_info();
}

// ----------------------------------------------------------------------------
void bitstamp_network::cancel_order(trade_data const &t)
{
    std::string data = "&id=" + std::to_string(t.id_);
    account_request("/api/v2/cancel_order/", std::move(data), [this](std::string &&data) {
        DEBUG_ONLY("Cancel Order response:\n" << data);
        // refresh order status
        get_open_orders();
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_buy_limit_order(trade_data const &t)
{
    std::string pair = std::string(to_string(t.currency_get_))
            + std::string(to_string(t.currency_pay_));
    //
    std::string data = "&amount=" + std::to_string(t.amount_get_)
            + "&price=" + std::to_string(t.amount_pay_);
    //
    account_request("/api/v2/buy/" + pair, std::move(data), [this](std::string &&data) {
        DEBUG_ONLY("Cancel Order response:\n" << data);
        // refresh order status
        get_open_orders();
    });
}
