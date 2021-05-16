#include <QApplication>
#include <QString>
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
    belle_https_bitstamp.address(bitstamp_https_address);
    belle_https_bitstamp.port(bitstamp_https_port);
    belle_https_bitstamp.ssl(true);
    belle_https_bitstamp.on_http_error([](auto& ctx)
    {
      std::cerr << "(belle_https_bitstamp) : General Error: "
                << ctx.ec.message() << "\n\n";
    });
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
        std::bind(&bitstamp_network::new_order_data, this, _1));
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
    return true;
}

// ----------------------------------------------------------------------------
void bitstamp_network::set_plot(OrderBookPlot *obp) {
    orderbook_ = new bitstamp_order_book(obp, false);
}

// ----------------------------------------------------------------------------
void bitstamp_network::request(const std::string &url_path, const std::string &url_query)
{
    app_settings* app_ini = global_settings();
    static std::string const url_host = bitstamp_https_address;

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
    std::string payload = url_encode("{offset:1}");

    std::string url_encoded = url_encode(url_path + url_query);
    std::string url_redirected = url_path + url_query;
    std::string http_method = "POST";

    // full query is signed using Hmac SHA256 algorithm
    std::string data_to_sign = "";
    data_to_sign.append(x_auth);
    data_to_sign.append(http_method);
    data_to_sign.append(url_host);
    data_to_sign.append(url_path);
    data_to_sign.append(url_query);
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
    b_request = OB::Belle::Request(http::verb::post, url_redirected, 11);
    b_request.target(url_redirected);
    b_request.set(http::field::host, url_host);
    b_request.set(http::field::content_type, content_type);
    b_request.set("X-Auth", x_auth);
    b_request.set("X-Auth-Signature", x_auth_signature);
    b_request.set("X-Auth-Nonce", x_auth_nonce);
    b_request.set("X-Auth-Timestamp", x_auth_timestamp);
    b_request.set("X-Auth-Version", x_auth_version);
    //
    b_request.body() = payload;
    b_request.prepare_payload();


    belle_https_bitstamp.on_http(b_request, [this](auto& ctx)
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
      this->account_data(std::move(ctx.res.body()));
    });

    /*auto completed = */belle_https_bitstamp.connect();

}

// ----------------------------------------------------------------------------
void bitstamp_network::new_order_data(bitstamp_network* n, std::string_view data)
{
    DEBUG_ONLY("\n\nReceived " << data << std::endl << std::endl);

    if (!n->orderbook_->accept_json_bitstamp(data))
        return;

    emit n->orderbook_changed();
}

// ----------------------------------------------------------------------------
void bitstamp_network::account_data(std::string&& data)
{
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
            "USD", "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B", currency_type::usd_bitstamp,
            std::stod(jdata["usd_balance"].get<std::string>()),
            std::stod(jdata["usd_available"].get<std::string>()),
            std::stod(jdata["usd_reserved"].get<std::string>()),
            nullptr
        };
        add_currency(usd_bitstamp, app_ini->bitstamp.currencies_);
    }

    if (jdata.contains("eur_balance")) {
        currency eur_bitstamp{
            "EUR", "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B", currency_type::eur_bitstamp,
            std::stod(jdata["eur_balance"].get<std::string>()),
            std::stod(jdata["eur_available"].get<std::string>()),
            std::stod(jdata["eur_reserved"].get<std::string>()),
            nullptr
        };
        add_currency(eur_bitstamp, app_ini->bitstamp.currencies_);
    }

    app_ini->bitstamp.widget_->set_data(app_ini->bitstamp);
}

// ----------------------------------------------------------------------------
void bitstamp_network::new_trade_data(bitstamp_network* n, std::string_view data)
{
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

    belle_https_bitstamp.on_http(req, [fn, repeat_ohlc](auto& ctx) mutable
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
    belle_https_bitstamp.connect();
}
