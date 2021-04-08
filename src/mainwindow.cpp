#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
//
#include <filesystem>
//
#include <boost/format.hpp>
//
#include "mainwindow.hpp"
#include "password_dialog.hpp"
#include "wallet_widget.hpp"
#include "currency_widget.hpp"
//
#include "src/internet/evp-encrypt.hpp"
#include "src/internet/https-async.hpp"
//
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
//
#include "ohlc.hpp"
#include "settings.hpp"
//
#include "hdf5.h"
//
#ifndef DEBUG_ONLY
# define DEBUG_ONLY(x)
# define DEBUG_ALWAYS(x) { \
    std::stringstream temp; temp << x; \
    std::cout << temp.str() << std::endl; }
#endif

// Main net rippled server
static const std::string ripple_mainnet_address = "s1.ripple.com";
static const int ripple_mainnet_port = 443;

// testnet rippled server
static const std::string ripple_testnet_address = "s.altnet.rippletest.net";
static const int ripple_testnet_port = 51233;

// mainnet data api
static const std::string ripple_dataapi_address = "data.ripple.com";
static const int ripple_dataapi_port = 443;

// testnet data api
static const std::string ripple_testapi_address = "testnet.data.api.ripple.com";
static const int ripple_testapi_port = 443;

//
static const std::string bitstamp_https_address = "www.bitstamp.net";
static const int bitstamp_https_port = 443;
//
static const std::string bitstamp_websocket_address = "ws.bitstamp.net";
static const int bitstamp_websocket_port = 443;

#ifdef GROX_USE_TESTNET
static const std::string ripple_network_address = ripple_testnet_address;
static const int ripple_network_port = ripple_testnet_port;
static const std::string ripple_data_api_address = ripple_testapi_address;
static const int ripple_data_api_port = ripple_testapi_port;
#else
static const std::string ripple_network_address = ripple_mainnet_address;
static const int ripple_network_port = ripple_mainnet_port;
static const std::string ripple_data_api_address = ripple_dataapi_address;
static const int ripple_data_api_port = ripple_dataapi_port;
#endif

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

namespace Belle = OB::Belle;
void on_http_error(Belle::Client& belle_https_connection)
{
  // set the http on error callback
  belle_https_connection.on_http_error([](auto& ctx)
  {
    std::cerr << "Error: " << ctx.ec.message() << "\n\n";
  });
}

// ----------------------------------------------------------------------------
GroxMainWindow::GroxMainWindow(QWidget* parent)
  : QMainWindow(parent)
{
    // qRegisterMetaType<currency>("currency");

    //
    // Build GUI from designer generated widgets/controls
    //
    ui.setupUi(this);
    //
    mainwindow = this;
    //
    repeat_ohlc_ = false;
    //
    // Create candlestick/volume plots
    //
    CombinedPriceVolumeCharts_ = new CombinedPriceVolumeCharts(this);
    priceAndPatternPlot_ = CombinedPriceVolumeCharts_->priceAndPatternPlot();
    ui.candlestick_layout->addWidget(CombinedPriceVolumeCharts_, 30);

    //
    // Create orderbook plot
    //
    obp = new OrderBookPlot(); // std::make_shared<OrderBookPlot>();
    obp->setMinimumSize(384,256);
    //obp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.order_plot_layout->addWidget(obp/*.get()*/, 0);
    bistamp_orderbook_ = new bitstamp_order_book(obp, false);

    // create xrp network interfaces
    xrpl_network_ = std::dynamic_pointer_cast<xrpl_network>(xrpl_network::get_xrpl_instance());
    xrpl_testnet_ = std::dynamic_pointer_cast<xrpl_network>(xrpl_network::get_xrpltestnet_instance());
    xrpl_network_->set_plot(obp);
    xrpl_testnet_->set_plot(obp);
    //
    // setup Qt actions/connections
    //
    createActions();
    createMenus();
    //
    read_hdf5();
    //
    // just an experiment to display an image
    //
    // scale pixmap to fit in label's size and keep ratio of pixmap
    QPixmap pix(":/images/xrp.jpg");
//    pix = pix.scaled(ui.image_label->size(), Qt::KeepAspectRatio);
//    ui.image_label->setPixmap(pix);
    //
    belle_https_bitstamp.address(bitstamp_https_address);
    belle_https_bitstamp.port(bitstamp_https_port);
    belle_https_bitstamp.ssl(true);
    on_http_error(belle_https_bitstamp);
    //
    AdjustingScrollArea *scroll = new AdjustingScrollArea(this);
    scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui.accounts_group->layout()->addWidget(scroll);
    //
    QFrame *frame = new QFrame(this);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setWidget(frame);
    //
    QVBoxLayout *vbox = new QVBoxLayout();
    frame->setLayout(vbox);
    //
    app_settings* app_ini = global_settings();
    {
        app_ini->bitstamp.widget_ = new wallet_widget(this);
        app_ini->bitstamp.widget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        app_ini->bitstamp.widget_->set_data(app_ini->bitstamp);
        vbox->addWidget(app_ini->bitstamp.widget_);
    }

    ranges::for_each(app_ini->xrpl_wallets, [&](auto &w) {
        // create a gui widget for the wallet
        w.widget_ = new wallet_widget(this);
        w.widget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        w.widget_->set_data(w);
        vbox->addWidget(w.widget_);
        // update wallet combo with name
        ui.xrp_acct_combo->addItem(QString(w.name_.c_str()));
        // updte networks with monitoried wallets
        if (w.testnet_) xrpl_testnet_->add_wallet(w);
        else xrpl_network_->add_wallet(w);
    });
    vbox->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    scroll->setWidgetResizable(true);
    scroll->adjustSize();
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
    delete CombinedPriceVolumeCharts_;
    delete bistamp_orderbook_;
    //
//    xrpl_network_.reset();
//    xrpl_testnet_.reset();
    //obp.reset();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::appExitCleanupHandler()
{
    qDebug() << "Main Window: appExitCleanupHandler()";
    // call clean up handlers of any components/widgets
    // block here to prevent access of temp buffers that are deleted
    // by the program/qt/etc
    qDebug() << "websockets: shutdown start";
    if (ws_trades)
        ws_trades->shutdown_blocking();
    if (ws_bidask)
        ws_bidask->shutdown_blocking();
    //
    xrpl_network_->disconnect();
    xrpl_network_.reset();
    xrpl_testnet_->disconnect();
    xrpl_testnet_.reset();
    //
    qDebug() << "websockets: shutdown complete";
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createActions()
{
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), this, SLOT(start_websocket()));
//    //
//    actionQuit = ui.menubar->addAction(tr("Quit"));
//    actionQuit->setMenuRole(QAction::QuitRole);
//    actionQuit->setShortcut(QKeySequence::Quit);
}

// ----------------------------------------------------------------------------
bool GroxMainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui.connect_button && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->modifiers() == Qt::ShiftModifier)
        {
            //do what you need
            DEBUG_ONLY("Shift click pressed");
            app_settings* app_ini = global_settings();
            std::array<std::string, 5> strings{
                app_ini->bitstamp.API_user, app_ini->bitstamp.API_key,
                app_ini->bitstamp.API_secret, std::to_string(app_ini->bitstamp.tag_),
                app_ini->bitstamp.public_};
            //
            password_dialog npw = password_dialog(strings, app_ini->xrpl_wallets);
            if (npw.exec() == QDialog::Accepted)
            {
                generate_encrypted_ini_data(npw);
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createMenus()
{
    ui.connect_button->installEventFilter(this);

//    connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui.connect_button, SIGNAL(clicked()), this, SLOT(start_websocket()));

    connect(this, SIGNAL(new_order_bitstamp_ui(QString)), ui.order_book_bitstamp,
        SLOT(setPlainText(QString)));

    connect(this, SIGNAL(update_arbitrage_view(QString)), ui.arbitrage_orders,
        SLOT(setPlainText(QString)));

    connect(this, SIGNAL(new_order_xrpl_ui(QString)), ui.order_book_xrpl,
        SLOT(setPlainText(QString)));

    //connect(this, SIGNAL(bitstamp_orderbook_replot()), bistamp_orderbook_->OrderBookPlot_.get(), SLOT(replot()));
    connect(this, SIGNAL(bitstamp_orderbook_replot()), this, SLOT(capture_image()));
    connect(this, SIGNAL(ledger_orderbook_replot()),
            xrpl_network_->get_orderbook()->OrderBookPlot_/*.get()*/, SLOT(replot()));

    connect(xrpl_network_.get(), SIGNAL(new_order_book_data(QString)), ui.order_book_xrpl,
            SLOT(setPlainText(QString)));

    connect(this, SIGNAL(new_ohlc_data_ui()), this, SLOT(new_ohlc_data()));
//    connect(this, SIGNAL(new_ledger_data()), this, SLOT(capture_image()));


    connect(ui.account_update, SIGNAL(clicked()), this, SLOT(update_account_balances()));


    connect(xrpl_network_.get(), SIGNAL(update_currency_widget(currency*)), this, SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
    connect(xrpl_testnet_.get(), SIGNAL(update_currency_widget(currency*)), this, SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
    connect(xrpl_network_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)), this, SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
    connect(xrpl_testnet_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)), this, SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::update_currency_widget(currency *c)
{
    assert(c->widget_);
    c->widget_->set_data(c);
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::update_wallet_widget(ledger_wallet *w)
{
    assert(w->widget_);
    w->widget_->set_data(*w);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_xrp()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", "Execute transaction?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        qDebug() << "Yes was clicked";
        app_settings* app_ini = global_settings();

        std::uint32_t tag = app_ini->bitstamp.tag_;
//        bool test = make_xrp_payment(ripple::KeyType::secp256k1,
//            app_ini->xrpl_wallets[app_ini->active_wallet].private_,
//            app_ini->xrpl_wallets[app_ini->active_wallet].public_,
//            app_ini->bitstamp.public_, tag, 10);

        QApplication::quit();
    } else {
        qDebug() << "Yes was *not* clicked";
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_usd()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", "Execute transaction?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        qDebug() << "Yes was clicked";
        QApplication::quit();
    } else {
        qDebug() << "Yes was *not* clicked";
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ticker_data(GroxMainWindow* mw, std::string&& data)
{
    DEBUG_ONLY("\n\nReceived " << data);

    if (data.rfind("{\"data\":", 0) != 0)
    {
        return;
    }
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
    emit mw->new_ticker_data_ui(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::merge_data(const QVector<QwtOHLCSample>& new_ohlc_samples,
    const std::vector<double>& new_ohlc_volumes)
{
    uint64_t update = 0;
    if (ohlc_samples.size() == 0)
    {
        ohlc_samples = new_ohlc_samples;
        ohlc_volumes = new_ohlc_volumes;
    }
    else if (!new_ohlc_samples.empty())
    {
        auto last_existing = ohlc_samples.back().time;
        auto first_new = new_ohlc_samples.front().time;

        DEBUG_ONLY("existing " << static_cast<uint64_t>(last_existing) << " new "
                  << static_cast<uint64_t>(first_new));
        if (first_new - last_existing == 60)
        {
            DEBUG_ONLY("merging data");
            ohlc_samples.append(new_ohlc_samples);
            ohlc_volumes.insert(
                ohlc_volumes.end(), new_ohlc_volumes.begin(), new_ohlc_volumes.end());
            if (ohlc_samples.size() != ohlc_volumes.size())
            {
                throw std::runtime_error("Data merge problem");
            }
            update = new_ohlc_samples.size();
        }
        else
        {
            throw std::runtime_error("Data OHLC time mismatch in merge");
        }
    }
    // write an update to the main datafile
    write_hdf5(ohlc_samples, ohlc_volumes, update);
    emit new_ohlc_data_ui();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::receive_ohlc_data(std::string&& data)
{
    try
    {
        // convert json data into vectors of actual data
        nlohmann::json jdata = json::parse(data)["data"]["ohlc"];
        std::vector<ohlc_string> ohlc_strings = jdata.get<std::vector<ohlc_string>>();
        //
        QVector<QwtOHLCSample> new_ohlc_samples;
        std::vector<double> new_ohlc_volumes;
        //
        const auto N = ohlc_strings.size();
        new_ohlc_samples.reserve(N);
        new_ohlc_volumes.reserve(N);
        //
        for (auto o : ohlc_strings)
        {
            ohlc temp(o);
            new_ohlc_samples.push_back(QwtOHLCSample(
                temp.timestamp, temp.open, temp.high, temp.low, temp.close));
            new_ohlc_volumes.push_back(temp.volume);
        }
        DEBUG_ONLY("Received " << ohlc_strings.size()
                  << " new OHLC samples");
        merge_data(new_ohlc_samples, new_ohlc_volumes);

        if (repeat_ohlc_)
        {
            request_new_candlestick_data();
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "JSON error decoding OHLC data: " << e.what() << "\n"
                  << data << std::endl << std::endl;
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ohlc_data()
{
    if (!ohlc_samples.empty())
        priceAndPatternPlot_->set_OHLC_data(ohlc_samples);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::capture_image()
{
    obp->update_time_and_replot();

    return;

//    auto image = ui.tabWidget->grab();
//    ui.imagelabel->setPixmap(image);
//    ui.imagelabel->setScaledContents(true);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_order_data(GroxMainWindow* mw, std::string_view data)
{
    DEBUG_ONLY("\n\nReceived " << data << std::endl << std::endl);

    if (mw->bistamp_orderbook_->accept_json_bitstamp(data))
        emit mw->bitstamp_orderbook_replot();

    //
    double budget = 100000;
    std::string arbitrage_string;
    double test_offset = 0.00;
    if (mw->ui.arbitrage_test_mode->isChecked()) {
        try {
            test_offset = std::stod(mw->ui.arbitrage_test_offset->text().toStdString());
        }
        catch (...) {
            test_offset = 0.00;
        }
    }

    //
    fee_data sell_fee{0.12, 0.0};
    fee_data buy_fee{0.0, 0.01};
    //
    if (mw->ui.enable_arbitrage->isChecked()) {
        mw->xrpl_network_->get_orderbook()->compute_arbitrage(*mw->bistamp_orderbook_, budget, buy_fee, sell_fee, test_offset, arbitrage_string);

        if (arbitrage_string.size()>0) {
            QString arb_string = QString::fromStdString(arbitrage_string);
            emit mw->update_arbitrage_view(arb_string);
        }
        else {
            emit mw->update_arbitrage_view("");
        }
    }

    QString datastring = QString::fromStdString(mw->bistamp_orderbook_->order_text);
    emit mw->new_order_bitstamp_ui(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::bitstamp_account_data(std::string&& data)
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
// this function not yet working
void GroxMainWindow::ledger_order_book(bool buy_xrp)
{
    // curl command to query : buy xrp for USD.bitstamp
    // curl -H 'Content-Type: application/json' -d '{"method":"book_offers","params":[{"taker_gets":{"currency":"XRP"},"taker_pays":{"currency":"USD","issuer":"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"},"limit":10}]}' https://s1.ripple.com:51234/
    // "{ \"command\": \"subscribe\", \"books\": [ { \"taker_pays\": { \"currency\": \"XRP\" }, \"taker_gets\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true }, { \"taker_gets\": { \"currency\": \"XRP\" }, \"taker_pays\": { \"currency\": \"USD\", \"issuer\": \"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B\" }, \"snapshot\": true } ] }",

    // init client with remote address, port, and ssl enabled
    Belle::Client app{ripple_network_address, ripple_network_port, true};
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

// ----------------------------------------------------------------------------
void GroxMainWindow::bitstamp_request(const std::string &url_path, const std::string &url_query)
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

    Belle::Request b_request;
    b_request = Belle::Request(http::verb::post, url_redirected, 11);
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


    belle_https_bitstamp.on_http(/*Belle::Method::post, */b_request, [this](auto& ctx)
    {
      // check http status code
      if (ctx.res.result() != Belle::Status::ok)
      {
        // print the response status code and reason
        std::cerr << "HTTPS Error: "
                  << ctx.res.result_int()
                  << " " << ctx.res.reason() << "\n\n";
        return;
      }
      // debug : print the response headers and body
      DEBUG_ONLY("Request response " << ctx.res.body() << "\n");
      this->bitstamp_account_data(std::move(ctx.res.body()));
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::update_account_balances()
{
    DEBUG_ONLY("Updating accounts");
    xrpl_network_->get_all_account_balances();
    xrpl_testnet_->get_all_account_balances();
    //
    bitstamp_request("/api/v2/balance/", "");
    /*auto completed = */belle_https_bitstamp.connect();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::start_websocket()
{
    using namespace std::placeholders;
    ws_trades = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"live_trades_xrpusd\"}}",
        std::bind(GroxMainWindow::new_ticker_data, this, _1));

    ws_bidask = net::ws::create_session(io_contexts.ioc, io_contexts.ctx,
        bitstamp_websocket_address, std::to_string(bitstamp_websocket_port),
        "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": "
        "\"order_book_xrpusd\"}}",
        std::bind(GroxMainWindow::new_order_data, this, _1));

    //
    xrpl_network_->subscribe_orderbook(io_contexts);
    xrpl_testnet_->subscribe_orderbook(io_contexts);
    xrpl_network_->subscribe_accounts(io_contexts);
    xrpl_testnet_->subscribe_accounts(io_contexts);

    // @TODO - should this go before subscriptions?
    // Run the I/O service on a thread.
    websocket_thread = std::thread([&]() {
        // The call will return when the socket is closed.
        io_contexts.ioc.run();
    });
    websocket_thread.detach();

    //
    request_new_candlestick_data();
    update_account_balances();
//    ledger_order_book(true);
//    ledger_order_book(false);
    /*auto completed = */
    belle_https_bitstamp.connect();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::request_new_candlestick_data(uint64_t /*unused*/)
{
    // what is the last sample we currently have
    uint64_t start_t = 0;
    if (ohlc_samples.size() > 0)
    {
        start_t = static_cast<uint64_t>(ohlc_samples.back().time);
        DEBUG_ONLY("Data present up until " << start_t);
        start_t += 60;    // next sample is 60s after last
    }
    //
    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    uint64_t unixtime = currentDateTime.toTime_t();
    //
    uint64_t diff = unixtime - start_t;
    uint64_t samples = diff / 60;
    //
    std::string req;
    repeat_ohlc_ = false;
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
            repeat_ohlc_ = true;
        }
        std::string start = std::to_string(start_t);
        std::string limit = std::to_string(samples);
        // send a request for ticker data using the io context thread to make the request
        req = "/api/v2/ohlc/xrpusd/?step=60&start=" + start + "&limit=" + limit;
    }

    belle_https_bitstamp.on_http(req, [this](auto& ctx)
    {
      // check http status code
      if (ctx.res.result() != Belle::Status::ok)
      {
        // print the response status code and reason
        std::cerr << "HTTPS Error: "
                  << ctx.res.result_int()
                  << " " << ctx.res.reason() << "\n\n";
        return;
      }
      // debug : print the response headers and body
      DEBUG_ONLY("Candlestick response " << ctx.res.body() << "\n");
      this->receive_ohlc_data(std::move(ctx.res.body()));
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::create_data_dir()
{
    namespace fs = std::filesystem;
    app_settings* app_ini = global_settings();
    if (!fs::exists(app_ini->appDataLocation))
    {
        if (!fs::create_directory(app_ini->appDataLocation))
        {
            throw std::runtime_error("Failed to create dir " + app_ini->appDataLocation);
        }
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::validate_ohlc()
{
    using cit = QVector<QwtOHLCSample>::const_iterator;
    cit li = ohlc_samples.begin();
    bool valid = true;
    uint64_t index = 0;
    for (cit i = ohlc_samples.begin() + 1; i != ohlc_samples.end(); ++i)
    {
        uint64_t t1 = static_cast<uint64_t>(li->time);
        uint64_t t2 = static_cast<uint64_t>(i->time);
        if (t2 - t1 != 60)
        {
            std::cerr << "Validation error at index " << index << " " << t1 << " and "
                      << t2 << "dataseet truncated " << std::endl;
            valid = false;
            break;
        }
        li = i;
        index++;
    }
    ohlc_samples.resize(index + 1);
    ohlc_volumes.resize(index + 1);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::read_hdf5()
{
    create_data_dir();
    //
    app_settings* app_ini = global_settings();
    //
    if (std::filesystem::exists(app_ini->hdfFileName))
    {
        DEBUG_ONLY("Opening: " << app_ini->hdfFileName);
        ohlc_samples.clear();
        ohlc_volumes.clear();
        //
        hid_t file = H5Fopen(app_ini->hdfFileName.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        // check if datasets exist
        if (H5Lexists(file, "ohlc", H5P_DEFAULT) > 0)
        {
            // read OHLC data
            hid_t dset1 = H5Dopen(file, "ohlc", H5P_DEFAULT);
            hid_t space1 = H5Dget_space(dset1);
            const int ndims1 = H5Sget_simple_extent_ndims(space1);
            hsize_t dims1[ndims1];
            herr_t status = H5Sget_simple_extent_dims(space1, dims1, NULL);
            //
            int N = dims1[0] / (sizeof(QwtOHLCSample) / sizeof(double));
            ohlc_samples.resize(N);
            status = H5Dread(dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                ohlc_samples.data());

            // read Volume data
            hid_t dset2 = H5Dopen(file, "volume", H5P_DEFAULT);
            hid_t space2 = H5Dget_space(dset2);
            const int ndims2 = H5Sget_simple_extent_ndims(space2);
            hsize_t dims2[ndims2];
            status = H5Sget_simple_extent_dims(space2, dims2, NULL);
            if (N != dims2[0])
            {
                throw std::runtime_error("Datasets not same size");
            }
            ohlc_volumes.resize(N);
            status = H5Dread(dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                ohlc_volumes.data());

            // free/close datasets
            status = H5Dclose(dset1);
            status = H5Dclose(dset2);
            // free/close dataspaces
            status = H5Sclose(space1);
            status = H5Sclose(space2);
        }
        // free/close file
        herr_t status = H5Fclose(file);
    }
    else
    {
        DEBUG_ONLY("Creating empty: " << app_ini->hdfFileName);

        // Create a new file using default properties.
        hid_t file_id = H5Fcreate(
            app_ini->hdfFileName.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        herr_t status = H5Fclose(file_id);
    }
    emit new_ohlc_data_ui();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::write_hdf5(const QVector<QwtOHLCSample>& samples,
    const std::vector<double>& volume, const uint64_t update)
{
    // check data before attempting to write to disk
    //    if (debug_level>0)
    validate_ohlc();
    //
    app_settings* app_ini = global_settings();
    DEBUG_ONLY("Opening: " << app_ini->hdfFileName);

    // In the OHLC dataset:
    // There are 24*60=1440 60s candles per day, and each candle has 5 {t,o,h,l,c} entries,
    // so the chunking size ought to be around 1440 * 5 = 7200 elements minimum,
    // a week's data will be 50400 doubles, so a nice binary number size for
    // chunking dimensions will be 65536
    //
    // The volume dataset has only 1 double per entry so we'll use a chunk dimension
    // 4 times less, of 16384
    //
    // Use unlimited size so that the data can be extended arbitrarily

    const uint64_t ohlc_size = sizeof(QwtOHLCSample) / sizeof(double);
    const uint64_t N = samples.size() * ohlc_size;
    hsize_t ohlc_dims[1] = {N};
    hsize_t vol_dims[1] = {volume.size()};
    hsize_t max_dims[1] = {H5S_UNLIMITED};
    hsize_t chunk_dim1[1] = {65536};
    hsize_t chunk_dim2[1] = {16384};
    herr_t status;

    // open the file, use UNLIMITED for main dimension so we can extend datasets
    hid_t file = H5Fopen(app_ini->hdfFileName.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);

    // create datasets if they do not exist already
    if (H5Lexists(file, "ohlc", H5P_DEFAULT) <= 0)
    {
        if (update != 0)
        {
            throw std::runtime_error("Cannot extend dataset before it exists");
        }
        // create a property list to set the chunking property on our OHLC dataset
        hid_t dprop1 = H5Pcreate(H5P_DATASET_CREATE);
        status = H5Pset_chunk(dprop1, 1, chunk_dim1);

        // write OHLC data,
        hid_t space1 = H5Screate_simple(1, ohlc_dims, max_dims);
        hid_t dset1 = H5Dcreate(
            file, "ohlc", H5T_IEEE_F64LE, space1, H5P_DEFAULT, dprop1, H5P_DEFAULT);
        status = H5Dwrite(
            dset1, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, samples.data());

        // create a property list to set the chunking property on our volume dataset
        hid_t dprop2 = H5Pcreate(H5P_DATASET_CREATE);
        status = H5Pset_chunk(dprop2, 1, chunk_dim2);

        // write Volume data
        hid_t space2 = H5Screate_simple(1, vol_dims, max_dims);
        hid_t dset2 = H5Dcreate(
            file, "volume", H5T_IEEE_F64LE, space2, H5P_DEFAULT, dprop2, H5P_DEFAULT);
        status = H5Dwrite(
            dset2, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, volume.data());
        // free/close datasets
        status = H5Dclose(dset1);
        status = H5Dclose(dset2);
        // free/close properties
        status = H5Pclose(dprop1);
        status = H5Pclose(dprop2);
        // free/close dataspaces
        status = H5Sclose(space1);
        status = H5Sclose(space2);
    }

    // if we are extending a dataset
    else if (update > 0)
    {
        DEBUG_ONLY("Extending datasets by: " << update);
        uint64_t offset = samples.size() - update;
        hsize_t offset1[1] = {offset * ohlc_size};
        hsize_t ext1[1] = {update * ohlc_size};
        hsize_t offset2[1] = {offset};
        hsize_t ext2[1] = {update};

        hid_t dset1 = H5Dopen(file, "ohlc", H5P_DEFAULT);
        // extend dataset to new size
        status = H5Dextend(dset1, ohlc_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace1 = H5Dget_space(dset1);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status = H5Sselect_hyperslab(fspace1, H5S_SELECT_SET, offset1, NULL, ext1, NULL);
        // Define memory space that we write our new data from
        hid_t dspace1 = H5Screate_simple(1, ext1, NULL);
        // Write new data to the hyperslab
        status = H5Dwrite(
            dset1, H5T_NATIVE_DOUBLE, dspace1, fspace1, H5P_DEFAULT, &samples[offset]);

        hid_t dset2 = H5Dopen(file, "volume", H5P_DEFAULT);
        // extend dataset to new size
        status = H5Dextend(dset2, vol_dims);
        // Select a hyperslab from the file dataspace
        hid_t fspace2 = H5Dget_space(dset2);
        // select hyperslab in new dataset : start, stride(NULL), count, block(NULL)
        status = H5Sselect_hyperslab(fspace2, H5S_SELECT_SET, offset2, NULL, ext2, NULL);
        // Define memory space that we write our new data from
        hid_t dspace2 = H5Screate_simple(1, ext2, NULL);
        // Write new data to the hyperslab
        status = H5Dwrite(
            dset2, H5T_NATIVE_DOUBLE, dspace2, fspace2, H5P_DEFAULT, &volume[offset]);

        // free/close datasets
        status = H5Dclose(dset1);
        status = H5Dclose(dset2);
        // free/close dataspaces
        status = H5Sclose(fspace1);
        status = H5Sclose(fspace2);
        status = H5Sclose(dspace1);
        status = H5Sclose(dspace2);
    }
    // free/close file
    status = H5Fclose(file);

    DEBUG_ONLY("Dataset size: " << ohlc_samples.size());
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
/*
void GroxMainWindow::xrp_dir_clicked() {
    if (ui.xrp_dir->arrowType()==Qt::ArrowType::DownArrow) {
        ui.xrp_dir->setArrowType(Qt::ArrowType::UpArrow);
    }
    else {
        ui.xrp_dir->setArrowType(Qt::ArrowType::DownArrow);
    }
}

void GroxMainWindow::usd_dir_clicked() {
    if (ui.usd_dir->arrowType()==Qt::ArrowType::DownArrow) {
        ui.usd_dir->setArrowType(Qt::ArrowType::UpArrow);
    }
    else {
        ui.usd_dir->setArrowType(Qt::ArrowType::DownArrow);
    }
}

void GroxMainWindow::transfer_setup_xrp(double fraction) {
    app_settings* app_ini = global_settings();
    if (ui.xrp_dir->arrowType()==Qt::ArrowType::UpArrow) {
        double amt = fraction * app_ini->bitstamp_xrp_available;
        ui.xrp_xfer->setText(boost::str(boost::format("%.6f") % amt).c_str());
    }
    else {
        double amt = fraction * app_ini->ledger_xrp_available;
        ui.xrp_xfer->setText(boost::str(boost::format("%.6f") % amt).c_str());
    }
}

void GroxMainWindow::transfer_setup_usd(double fraction) {
    app_settings* app_ini = global_settings();
    if (ui.usd_dir->arrowType()==Qt::ArrowType::UpArrow) {
        double amt = fraction * app_ini->bitstamp_usd_available;
        ui.usd_xfer->setText(boost::str(boost::format("%.6f") % amt).c_str());
    }
    else {
        double amt = fraction * app_ini->ledger_usd_available;
        ui.usd_xfer->setText(boost::str(boost::format("%.6f") % amt).c_str());
    }
}
*/

// ----------------------------------------------------------------------------
