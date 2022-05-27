#include <QString>
//
#include <string>
//
#include "src/debug.hpp"
#include "src/exchange/xrpl_network.hpp"
#include "src/network/https-async.hpp"
#include "src/network/websocket-ssl.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/widgets/wallet_widget.hpp"
//
#include "src/exchange/bitstamp.hpp"
//
// ----------------------------------------------------------------------------
bitstamp_network::bitstamp_network()
{
    bitstamp_account default_acct;
    default_acct.name_ = "Bitstamp Main";
    accounts_.push_back(default_acct);
}

// ----------------------------------------------------------------------------
bitstamp_network::~bitstamp_network()
{
    qDebug() << "bitstamp_network: destructor";
    delete orderbook_;
}

// ----------------------------------------------------------------------------
bool bitstamp_network::subscribe_live_trades(net::contexts &io_contexts)
{
    using namespace std::placeholders;
    DEBUG_ALWAYS("Subscribing to live_trades_xrpusd");
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
    DEBUG_ALWAYS("Subscribing to order_book_xrpusd");
    ws_bidask = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"order_book_xrpusd\"}}",
        std::bind(&bitstamp_network::new_orderbook_data, this, _1));

    return true;
}
/*
// ----------------------------------------------------------------------------
bool bitstamp_network::unsubscribe_channel(net::contexts &io_contexts, network::streams stream)
{
    "event": "bts:unsubscribe",
    "data": {
        "channel": "[channel_name]"
    }
}
*/
// ----------------------------------------------------------------------------
// connect to (multiple) streams
bool bitstamp_network::connect(net::contexts &io_contexts, streams_vector const &streams)
{
    bool ok = true;
    for (const auto &s : streams) {
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
        if (s == network::streams::trades) ws_trades->shutdown_blocking();
        if (s == network::streams::order_book) ws_bidask->shutdown_blocking();
    }
    return ok;
}

// ----------------------------------------------------------------------------
void bitstamp_network::shut_down()
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
exchange::currency_pairlist bitstamp_network::currency_pairs()
{
    currency c1 = currency{{currency::bitstamp_trust, "USD"}, currency_type::usd_bitstamp, 0, 0, 0, nullptr};
    currency c2 = currency{{"", "XRP"}, currency_type::xrp, 0, 0, 0, nullptr};
    currency c3 = currency{{currency::bitstamp_trust, "EUR"}, currency_type::eur_bitstamp, 0, 0, 0, nullptr};
    currency_pairlist supported = {{c1,c2}, {c2,c1}, {c3,c2}, {c2,c3}};
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

std::string lowercase(std::string data) {
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return data;
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
void bitstamp_network::get_account_info()
{
    account_request("/api/v2/balance/", "", [this](std::string &&data) {
        handle_account_info(std::move(data));
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_account_info(std::string&& data)
{
    DEBUG_ONLY("bitstamp account_info : thread " << std::this_thread::get_id());
    DEBUG_ONLY("Response : " << data);
    //
    nlohmann::json jdata = json::parse(data);
    DEBUG_ONLY(jdata.dump(4));
    //
    bitstamp_account &acct = get_bitstamp_instance()->account();

    currency xrp_bitstamp{
        {"", "XRP"}, currency_type::xrp,
        std::stod(jdata["xrp_balance"].get<std::string>()),
        std::stod(jdata["xrp_available"].get<std::string>()),
        std::stod(jdata["xrp_reserved"].get<std::string>()),
        nullptr
    };
    acct.add_currency(xrp_bitstamp);

    if (jdata.contains("usd_balance")) {
        currency usd_bitstamp{
            {currency::bitstamp_trust, "USD"}, currency_type::usd_bitstamp,
            std::stod(jdata["usd_balance"].get<std::string>()),
            std::stod(jdata["usd_available"].get<std::string>()),
            std::stod(jdata["usd_reserved"].get<std::string>()),
            nullptr
        };
        acct.add_currency(usd_bitstamp);
    }

    if (jdata.contains("eur_balance")) {
        currency eur_bitstamp{
            {currency::bitstamp_trust, "EUR"}, currency_type::eur_bitstamp,
            std::stod(jdata["eur_balance"].get<std::string>()),
            std::stod(jdata["eur_available"].get<std::string>()),
            std::stod(jdata["eur_reserved"].get<std::string>()),
            nullptr
        };
        acct.add_currency(eur_bitstamp);
    }

    if (jdata.contains("xrpusd_fee")) {
        double xrpusd_fee = std::stod(jdata["xrpusd_fee"].get<std::string>());
        std::pair<std::string, std::string> cpair = std::make_pair("xrp", "usd");
        const auto [it, success] = fee_map_.insert({cpair, xrpusd_fee});
        if (success) {
            std::cout << "Inserted bitstamp fee xrp/usd " << xrpusd_fee << std::endl;
        }
        else {
            std::cout << "Overwriting bitstamp fee xrp/usd " << xrpusd_fee << std::endl;
            fee_map_[cpair] = xrpusd_fee;
        }
    }

    if (jdata.contains("xrpeur_fee")) {
        double xrpeur_fee = std::stod(jdata["xrpeur_fee"].get<std::string>());
        std::pair<std::string, std::string> cpair = std::make_pair("xrp", "eur");
        const auto [it, success] = fee_map_.insert({cpair, xrpeur_fee});
        if (success) {
            std::cout << "Inserted bitstamp fee xrp/eur " << xrpeur_fee << std::endl;
        }
        else {
            std::cout << "Overwriting bitstamp fee xrp/eur " << xrpeur_fee << std::endl;
            fee_map_[cpair] = xrpeur_fee;
        }
    }
    emit update_wallet_widget(&acct);
}

// ----------------------------------------------------------------------------
void bitstamp_network::get_open_orders()
{
    account_request("/api/v2/open_orders/all/", "", [this](std::string &&data) {
        DEBUG_ONLY("Open Order response:\n" << data);
        handle_open_orders(std::move(data));
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::handle_open_orders(std::string&& data)
{
    bitstamp_account &acct = get_bitstamp_instance()->account();
    auto &trades = acct.offers_;
    trades.clear();
    //
    nlohmann::json jdata = json::parse(data);
    for (auto& [key, val] : jdata.items())
    {
        std::string cs = val["currency_pair"];
        std::size_t pos = cs.find("/");
        std::string c1 = cs.substr(0,pos);
        std::string c2 = cs.substr(pos+1);

        double amount = std::stod(val["amount"].get< std::string >());
        double price = std::stod(val["price"].get< std::string >());
        double fee_percent = 0;
        double fee_fixed = 0;
        // 0=buy, 1=sell
        auto trade_type_ = (val["type"] == "0") ? trade_type::buy : trade_type::sell;

        if (trade_type_ == trade_type::sell) {
            trade_data t{
                this->get_instance(),
                account().name_,
                get_currency_type({"", c2}),
                get_currency_type({"", c1}),
                amount*price,
                amount,
                price,
                fee_percent,
                fee_fixed,
                std::stoull(val["id"].get< std::string >()),
                val["datetime"]
            };
            trades.push_back(t);
        }
        else {
            trade_data t{
                this->get_instance(),
                account().name_,
                get_currency_type({"", c1}),
                get_currency_type({"", c2}),
                amount,
                amount*price,
                price,
                fee_percent,
                fee_fixed,
                std::stoull(val["id"].get< std::string >()),
                val["datetime"]
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
        DEBUG_ONLY("Request " << b_request << "\n");

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

    live_trades trade_data = jdata.get<live_trades>();

    //    emit candlestickdata(ohlc_vector);

    emit n->new_trade_data_ui(trade_data);
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
    std::transform(req.begin(), req.end(), req.begin(),
        [](unsigned char c){ return std::tolower(c); });
    //
    DEBUG_ALWAYS("Placing order " << req << " " << data);
    account_request(std::move(req), std::move(data), [this, update_after](std::string &&data) {
        DEBUG_ALWAYS("Buy-Limit Order response:\n" << data);
        // refresh order status
        if (update_after) get_open_orders();
    });
}

// ----------------------------------------------------------------------------
void bitstamp_network::place_buy_sell_orders(basic_account *acct, std::vector<trade_data> const &trades)
{
    for (auto const &t : trades) {
        if (&t != &trades.back()) place_limit_order(t, false);
        else place_limit_order(t, true);
    }
}
