// STL
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>
// Qt
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
#include <QDockWidget>
#include <QScrollBar>
// Qwt
#include <QwtAxis>
#include <QwtOHLCSample>
#include <QwtScaleDraw>
#include <QwtScaleEngine>
// Grox
#include "mainwindow.hpp"
#include "src/widgets/password_dialog.hpp"
#include "src/widgets/wallet_widget.hpp"
#include "src/widgets/currency_widget.hpp"
#include "src/widgets/trade_widget.hpp"
#include "src/widgets/check_trades_dialog.hpp"
#include "src/widgets/trade_algorithm.hpp"
#include "src/widgets/digital_clock.hpp"
//
#include "src/demangle_helper.hpp"
#include "src/print.hpp"
#include "src/network/evp-encrypt.hpp"
#include "src/network/https-async.hpp"
//
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
//
#include "json_types.hpp"
#include "settings.hpp"
#include "print.hpp"
//
#include "src/stream/trade_filter.hpp"
#include "src/data/ohlc_heikin_ashi.hpp"

#define get_live_trades 1
//#define subscribe_xrpl_events 1

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> main_dbg("Mainwin");

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
    // Create candlestick/volume plots
    //
    crypto_price_plot_ = new ohlc_price_plot(this, &hdf5_ohlc_);
    ui.candlestick_layout->addWidget(crypto_price_plot_, 30);

    //
    // Create stream/filters plot
    //
    assets_plot_ = new filter_plot(this);
    ui.filters_layout->addWidget(assets_plot_, 30);
    assets_plot_->setMinimumHeight(128);
    assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

    filters_plot_ = new filter_plot(this);
    ui.filters_layout->addWidget(filters_plot_, 30);
    filters_plot_->setMinimumHeight(128);
    filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);


    // ----------------------------------
    // Create orderbook plot
    //
    obp_ = nullptr;
    obp_ = new OrderBookPlot();
    obp_->setMinimumSize(384,256);
    //
    ui.order_plot_layout->addWidget(obp_, 0);

    // ----------------------------------
    // create bitstamp exchange interface
    //
    bitstamp_network_ = bitstamp_network::get_bitstamp_instance();
    bitstamp_network_->set_plot(obp_);

    // ----------------------------------
    // create xrp network interfaces
    // we do not plot the xrp testnet orderbook
    //
    xrpl_network_ = xrpl_network::get_xrpl_instance(false);
    xrpl_testnet_ = xrpl_network::get_xrpl_instance(true);
    xrpl_network_->set_plot(obp_);

    exchange_list_.push_back(bitstamp_network_);
    exchange_list_.push_back(xrpl_network_);
    exchange_list_.push_back(xrpl_testnet_);

    // timer will fire once each time it is reset
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    candlestick_update_active_ = false;

    // ----------------------------------
    // setup Qt actions/connections
    //
    createActions();
    createMenus();

    // ----------------------------------
    app_settings* app_ini = global_settings();

    // ----------------------------------
    // Load existing candlestick data
    hdf5_ohlc_.init(app_ini->appDataLocation, app_ini->hdfFileName);
    hdf5_ohlc_.read_hdf5();

    // get all available candle resolutions, except highest res
    // since we we use that one to generate all the others
    const auto &resolutions = ohlc_chart_data::available_resolutions();
    for (size_t i=1; i<resolutions.size(); ++i) {
        auto const &res = resolutions[i];
        auto new_data = hdf5_ohlc_.get_dataset(res.base_)->resample(res, ohlc_chart_data::get_resolution(res.base_));
        if (new_data) {
            hdf5_ohlc_.add_dataset(res, new_data);
        }
    }

    crypto_price_plot_->set_data(&hdf5_ohlc_);
    // start by displaying 1 day of data
    graph_rescale(0);

    // ----------------------------------
    // just an experiment to display an image
    //
    // scale pixmap to fit in label's size and keep ratio of pixmap
    QPixmap pix(":/images/xrp.jpg");
    // pix = pix.scaled(ui.image_label->size(), Qt::KeepAspectRatio);
    // ui.image_label->setPixmap(pix);

    // ----------------------------------
    // create a dock widget to hold accounts/wallets
    //
    accounts_scrollwidget = new QScrollArea(this);
    accounts_scrollwidget->setWidgetResizable(true);
    QFrame *accounts_frame = new QFrame(accounts_scrollwidget);
    accounts_frame->setLayout(new QVBoxLayout());
    accounts_scrollwidget->setWidget(accounts_frame);
    accounts_dock = std::make_shared<QDockWidget>("Accounts", this);
    accounts_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    accounts_dock->setFeatures(
            QDockWidget::DockWidgetClosable |
            QDockWidget::DockWidgetMovable |
            QDockWidget::DockWidgetFloatable);
    accounts_dock->setObjectName("AccountsDock");
    accounts_dock->setWidget(accounts_scrollwidget);
    addDockWidget(Qt::RightDockWidgetArea, accounts_dock.get());

    // ----------------------------------
    // create a dock widget to hold trade orders
    //
    orders_scrollwidget = new QScrollArea(this);
    orders_scrollwidget->setWidgetResizable(true);
    QFrame *main_frame = new QFrame(orders_scrollwidget);
    main_frame->setLayout(new QVBoxLayout());
    orders_scrollwidget->setWidget(main_frame);
    orders_dock = std::make_shared<QDockWidget>("Orders", this);
    orders_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    orders_dock->setFeatures(
            QDockWidget::DockWidgetClosable |
            QDockWidget::DockWidgetMovable |
            QDockWidget::DockWidgetFloatable);
    orders_dock->setObjectName("OrdersDock");
    orders_dock->setWidget(orders_scrollwidget);
    addDockWidget(Qt::RightDockWidgetArea, orders_dock.get());

    // for each wallet on each network
    for (auto network : app_ini->networks_) {
        for (auto w : network->wallets()) {
            // create a gui widget for the wallet
            w->widget_ = new wallet_widget(this);
            w->widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            if (network->name()=="Bitstamp") w->widget_->set_data(*static_cast<bitstamp_account*>(w));
            if (network->name()=="XRPL")     w->widget_->set_data(*static_cast<ledger_wallet*>(w));
            accounts_frame->layout()->addWidget(w->widget_);
            // update wallet combo with name
            ui.all_acct_combo->addItem(QString(w->name_.c_str()));
        }
    }
    accounts_frame->layout()->addItem(new QSpacerItem(1,1, QSizePolicy::Expanding, QSizePolicy::Preferred));

    // load in the list of trustlines that we know about
    loadTrustlines();

    update_account_balances();

    //
    // Resize order book to fit monospace text (add 1 chars - scrollbars/etc)
    //
    QString txt = "X";
    int char_size = QFontMetrics(ui.order_book_xrpl->font()).horizontalAdvance(txt);
    int calcWidth = char_size*85 + 8;
    //std::cout << "width", ui.order_book_xrpl->verticalScrollBar()->geometry().width() << std::endl;
    ui.order_book_xrpl->setMinimumWidth(calcWidth);
    //ui.order_book_xrpl->setMaximumWidth(calcWidth);
    ui.order_book_bitstamp->setMinimumWidth(calcWidth);
    //ui.order_book_bitstamp->setMaximumWidth(calcWidth);
    //
    calcWidth = char_size*140 + 8;
    ui.arbitrage_orders->setMinimumWidth(calcWidth);
    //ui.arbitrage_orders->setMaximumWidth(calcWidth);

    QStringList slist("Auto");
    for (const auto &r : ohlc_chart_data::available_resolutions()) {
        slist << r.name_;
    }
    ui.candle_res->addItems(slist);
    ui.candle_res_2->addItems(slist);

    // ----------------------------------
    // setup connections tab
    loadConnectionSetups();
    connection_widget_ = new connection_widget(io_contexts_, exchange_list_, ui.connections_tab);
    ui.connections_tab->layout()->addWidget(connection_widget_);

    DigitalClock *clock = new DigitalClock(this);
    ui.controls_layout->addWidget(clock);
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
    delete timer_;
    delete crypto_price_plot_;
    delete obp_;
}

// ----------------------------------------------------------------------------
void GroxMainWindow::appExitCleanupHandler()
{
    main_dbg<0>.debug(str<>("appExitCleanupHandler"));
    // call clean up handlers of any components/widgets
    // block here to prevent access of temp buffers that are deleted
    // by the program/qt/etc
    //
    bitstamp_network_->shut_down();
    bitstamp_network_.reset();
    //
    xrpl_network_->shut_down();
    xrpl_network_.reset();
    //
    xrpl_testnet_->shut_down();
    xrpl_testnet_.reset();
    main_dbg<0>.debug(str<>("websockets"), "shutdown complete");

    // remove work guard so IO threads can exit
    io_contexts_.work_guard_->reset();

    // stop boost::asio io_service
    io_contexts_.ioc.stop();
    for (auto &t : ioc_threads_) {
        if (t.joinable()) t.join();
    }
    main_dbg<0>.debug(str<>("boost::asio"), "shutdown complete");
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createActions()
{
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_D), this, SLOT(restore_dockwindows()));

    // Ctrl+R : shows how to connect a lambda to a shortcut
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_R),  this), &QShortcut::activated, [=](){
    });

    start_io_threads(2);

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
            main_dbg<6>.debug("Shift click pressed");
            std::array<std::string, 5> strings{
                bitstamp_network_->account().API_user,
                bitstamp_network_->account().API_key,
                bitstamp_network_->account().API_secret,
                std::to_string(bitstamp_network_->account().tag_),
                bitstamp_network_->account().public_};

            // iterate over wallets to convert type from basic pointers
            // @TODO - improve this
            std::vector<ledger_wallet> wallets;
            auto x1 = xrpl_network::get_xrpl_instance(false)->wallets();
            auto x2 = xrpl_network::get_xrpl_instance(true)->wallets();
            ranges::for_each(x1, [&](basic_account* b){
                ledger_wallet w = *static_cast<ledger_wallet*>(b);
                wallets.push_back(w);
            });
            ranges::for_each(x2, [&](basic_account* b){
                ledger_wallet w = *static_cast<ledger_wallet*>(b);
                wallets.push_back(w);
            });
            password_dialog npw = password_dialog(strings, wallets);
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
    // to capture ctrl-click on connect button
    ui.connect_button->installEventFilter(this);

    // action for quit (@TODO)
    // connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));

    // button-click : fetch latest account balance data
    connect(ui.account_update, SIGNAL(clicked()), this, SLOT(update_account_balances()));

    // when new candlestick data is ready, redo main graph
    connect(this, SIGNAL(new_ohlc_data_ui(double)), this, SLOT(new_ohlc_data(double)));

    // ---------------------------------------------------------------------
    // signals emitted from networking thread completion handlers should use
    // Qt::QueuedConnection to ensure they transfer to Qt main thread
    // ---------------------------------------------------------------------

    // used to fetch account balances after N seconds
    connect(timer_, SIGNAL(timeout()), this, SLOT(candlestick_timer_event()));

    // Timers must be started from the qt thread that created them
    connect(this, SIGNAL(restart_candlestick_timer()), this, SLOT(restart_candlestick_timer_event()));

    // orderbook updates from bitstamp network connection
    // 1 Priority, arbitrage, 2 plot update, 3 text update
    connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()),
            this, SLOT(perform_arbitrage()), Qt::QueuedConnection);
    connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()),
            obp_, SLOT(update_time_and_replot()), Qt::QueuedConnection);
    connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()),
            this, SLOT(orderbook_text_update()), Qt::QueuedConnection);

    // when a transaction takes place we might need to update wallet/records
    connect(bitstamp_network_.get(), &bitstamp_network::transaction_event, this, [this]() {
        transaction_event();
        display_offers();
    } , Qt::QueuedConnection);

    connect(bitstamp_network_.get(), &bitstamp_network::update_wallet_widget, this, [this](bitstamp_account *acct) {
        acct->widget_->set_data(*acct);
        display_offers();
    } , Qt::QueuedConnection);

    connect(bitstamp_network_.get(), &bitstamp_network::new_trade_data_ui, this, [this](live_trades t) {
        auto p = t.price;
        auto v = t.amount;
        QwtOHLCSample new_sample(1000.0*std::atof(t.timestamp.c_str()), p, p, p, p, v);
        hdf5_ohlc_.add_live_data(new_sample);
        //
        crypto_price_plot_->update_live_data(new_sample);
        stream_process(new_sample);
    } , Qt::QueuedConnection);

    connect(xrpl_network_.get(), SIGNAL(update_currency_widget(currency*)),
            this, SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
    connect(xrpl_testnet_.get(), SIGNAL(update_currency_widget(currency*)),
            this, SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
    connect(xrpl_network_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)),
            this, SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
    connect(xrpl_testnet_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)),
            this, SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
    connect(xrpl_network_.get(), SIGNAL(orderbook_changed()),
            obp_, SLOT(update_time_and_replot()), Qt::QueuedConnection);
    connect(xrpl_network_.get(), SIGNAL(orderbook_changed()),
            this, SLOT(orderbook_text_update()), Qt::QueuedConnection);

    // when a transaction takes place we might need to update wallet/records
    connect(xrpl_network_.get(), SIGNAL(transaction_event()),
            this, SLOT(transaction_event()), Qt::QueuedConnection);


    connect(ui.gt_6, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(-2);
    } , Qt::QueuedConnection);
    connect(ui.gt_12, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(-1);
    } , Qt::QueuedConnection);
    connect(ui.gt_d, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(0);
    } , Qt::QueuedConnection);
    connect(ui.gt_w, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(1);
    } , Qt::QueuedConnection);
    connect(ui.gt_m, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(2);
    } , Qt::QueuedConnection);
    connect(ui.gt_y, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(3);
    } , Qt::QueuedConnection);
    connect(ui.gt_a, &QAbstractButton::clicked, this, [this]() {
        graph_rescale(4);
    } , Qt::QueuedConnection);

    connect(ui.exec_algo, &QAbstractButton::clicked, this, [this]() {
        execute_filter();
    } , Qt::QueuedConnection);

    connect(ui.candle_res, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        if (index>0) {
            double res = ohlc_chart_data::available_resolutions()[index-1];
            crypto_price_plot_->set_auto_candle_resolution(false);
            if (crypto_price_plot_->adjust_candle_size(res)) {
                crypto_price_plot_->adjust_data_scaling();
            }
            crypto_price_plot_->replot();
        }
        else {
            crypto_price_plot_->set_auto_candle_resolution(true);
            if (crypto_price_plot_->adjust_candle_size(0)) {
                crypto_price_plot_->adjust_data_scaling();
            }
            crypto_price_plot_->replot();
        }
    } , Qt::QueuedConnection);

    connect(ui.heikin, QOverload<int>::of(&QCheckBox::stateChanged), this, [this](int state){
        if (state) {
            crypto_price_plot_->setMode(ohlc_chart_curve::HeikinAshi);
        }
        else {
            crypto_price_plot_->setMode(QwtPlotTradingCurve::SymbolStyle::CandleStick);
        }
    } , Qt::QueuedConnection);

    connect(crypto_price_plot_->get_interactor(), &ohlc_interactor::repair_pressed, this, [this](QPointF p) {
        auto crosshairs = crypto_price_plot_->get_crosshairs();
        double msecs = crosshairs->quantize_x_coord(p.x());
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
        QString s = QLocale::system().toString(dt, "dd-MM-yy hh:mm");

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Confirm", "Delete from " + s,
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            hdf5_ohlc_.truncate_from_time(msecs);
            update_candlestick_data();
        } else {
            main_dbg<0>.debug(str<>("Yes *not* clicked"));
        }
    } , Qt::QueuedConnection);

    connect(crypto_price_plot_, &ohlc_price_plot::plotScaleChanged, this, [this](double t1, double t2) {
        filters_plot_->update_time_axis(t1, t2);
        assets_plot_->update_time_axis(t1, t2);

//                axisScaleDraw(QwtPlot::xBottom)->, crypto_price_plot_->axisScaleDraw(QwtPlot::xBottom));
        //    filters_plot_->setAxisScaleEngine(QwtPlot::xBottom, crypto_price_plot_->axisScaleEngine(QwtPlot::xBottom));
    } , Qt::QueuedConnection);

    connect(ui.run_filter, &QPushButton::clicked, this, [this]() {
        execute_filter();
    } , Qt::QueuedConnection);
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::graph_rescale(int range)
{
    auto last_time = hdf5_ohlc_.get_last_sample_time(true);
    double t1=0, t2 = last_time;
    if (range==-2) {
        t1 = last_time - 0.25*ohlc_chart_data::day;
    }
    else if (range==-1) {
        t1 = last_time - 0.5*ohlc_chart_data::day;
    }
    else if (range==0) {
        t1 = last_time - 1.0*ohlc_chart_data::day;
    }
    else if (range==1) {
        t1 = last_time - 7*ohlc_chart_data::day;
    }
    else if (range==2) {
        t1 = last_time - 31*ohlc_chart_data::day;
    }
    else if (range==3) {
        t1 = last_time - 365*ohlc_chart_data::day;
    }
    // special case, to extend current view with new data
    else if (range==100) {
        t1 = last_time - 365*ohlc_chart_data::day;
    }
    else {
        t1 = hdf5_ohlc_.get_first_sample_time();
    }
    crypto_price_plot_->update_time_axis(t1, t2);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ohlc_data(double old_res)
{
    (void)(old_res);
    main_dbg<5>.debug(str<>("new ohlc data"), "resolution", old_res);
    //
    // get all available candle resolutions, except highest res
    // since we we use that one to generate all the others
    const auto &resolutions = ohlc_chart_data::available_resolutions();
    for (size_t i=1; i<resolutions.size(); ++i) {
        auto const &res = resolutions[i];
        auto data = hdf5_ohlc_.get_dataset(res);
        if (data) {
            data->resample_update(res, hdf5_ohlc_.get_dataset(res.base_), ohlc_chart_data::get_resolution(res.base_));
        }
        else {
            data = hdf5_ohlc_.get_dataset(res.base_)->resample(res, ohlc_chart_data::get_resolution(res.base_));
            hdf5_ohlc_.add_dataset(res, data);
        }
    }

    // don't change axes, just update data series and replot
    crypto_price_plot_->replot();
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
    display_offers();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_xrp()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", "Execute transaction?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        main_dbg<0>.debug(str<>("Yes clicked"));
//        app_settings* app_ini = global_settings();

//        std::uint32_t tag = bitstamp_network_->account().tag_;
//        bool test = make_xrp_payment(ripple::KeyType::secp256k1,
//            app_ini->xrpl_wallets[app_ini->active_wallet].private_,
//            app_ini->xrpl_wallets[app_ini->active_wallet].public_,
//            bitstamp_network_->account().public_, tag, 10);

        QApplication::quit();
    } else {
        main_dbg<0>.debug(str<>("Yes *not* clicked"));
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_usd()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", "Execute transaction?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        main_dbg<0>.debug(str<>("Yes clicked"));
        QApplication::quit();
    } else {
        main_dbg<0>.debug(str<>("Yes *not* clicked"));
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::receive_ohlc_data(std::string&& data)
{
    try
    {
        // convert json data into vectors of actual data
        nlohmann::json jdata = json::parse(data)["data"]["ohlc"];
        main_dbg<0>.debug(str<>("Received"), jdata.size(), "json OHLC samples");
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
        main_dbg<5>.debug("Converted", new_ohlc_samples.size(), "new OHLC samples");
        hdf5_ohlc_.merge_data(ohlc_chart_data::minute, new_ohlc_samples);
        // what is the last sample we currently have
        auto last_time = hdf5_ohlc_.get_last_sample_time(false);
        main_dbg<0>.debug(str<>("Data merged up to"), msecs_unix_to_calendar_time(last_time));
        hdf5_ohlc_.delete_live_data_up_to(last_time);
        //
        emit new_ohlc_data_ui(ohlc_chart_data::minute);

    }
    catch (std::exception& e)
    {
        main_dbg<0>.error(str<>("JSON error"), "decoding OHLC data:", e.what(), "\n", data, "\n\n");
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::capture_image()
{
    obp_->update_time_and_replot();
    return;

//    auto image = ui.tabWidget->grab();
//    ui.imagelabel->setPixmap(image);
//    ui.imagelabel->setScaledContents(true);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::update_account_balances()
{
    main_dbg<5>.debug("Updating accounts");
    //
    bitstamp_network_->get_account_info();
    bitstamp_network_->get_open_orders();
    //
    xrpl_network_->get_all_account_infos();
    xrpl_network_->get_all_account_lines();
    xrpl_network_->get_all_account_offers();
    //
    if (true) {
        xrpl_testnet_->get_all_account_infos();
        xrpl_testnet_->get_all_account_lines();
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::update_candlestick_data()
{
    candlestick_update_active_ = true;
    uint64_t req_t = 0, start_t = 0;
    // what is the most recent sample we currently have
    start_t = static_cast<uint64_t>(hdf5_ohlc_.get_last_sample_time(false));
    if (start_t == 0) {
        // linux time 1496275200 = Thu Jun 01 2017 00:00:00 GMT+0000
        start_t = 1496275200*1000.0;
        std::string s = msecs_unix_to_calendar_time(start_t);
        main_dbg<5>.debug(str<>("No Data"),  "requesting from", s);
    }
    else {
        std::string s = msecs_unix_to_calendar_time(start_t);
        main_dbg<5>.debug(str<>("Data present until"), s);
    }
    // convert to unix timestamp : next sample is 60s after last
    req_t = start_t/1000 + 60;

    main_dbg<5>.debug(str<>("Requesting candlesticks"), msecs_unix_to_calendar_time(req_t*1000));

    // @TODO add futures here to make dependency chain simpler?
    bitstamp_network_->request_new_candlestick_data(req_t, [this, req_t](auto& ctx, bool more) {
        main_dbg<0>.debug(str<>("Received"), msecs_unix_to_calendar_time(req_t*1000));
        this->receive_ohlc_data(std::move(ctx.res.body()));
        if (more) {
            update_candlestick_data();
        }
        else {
            candlestick_update_active_ = false;
            emit restart_candlestick_timer();
        }
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::start_io_threads(int nthreads)
{
    static bool initialized = false;
    if (!initialized) {
        ioc_threads_.reserve(nthreads);
        // Run the I/O service on some threads.
        for (int i=0; i<nthreads; ++i) {
            ioc_threads_.emplace_back([&]() {
                main_dbg<5>.debug(str<>("io_contexts"), "run : thread", std::this_thread::get_id());
                // The call will return when the socket is closed.
                io_contexts_.ioc.run();
            });
        }
        initialized = true;
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::restore_dockwindows()
{
    accounts_dock->show();
    orders_dock->show();
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void GroxMainWindow::perform_arbitrage()
{
    double budget = 100000;
    std::string arbitrage_string;
    double test_offset = 0.00;
    if (ui.arbitrage_test_mode->isChecked()) {
        try {
            test_offset = std::stod(ui.arbitrage_test_offset->text().toStdString());
        }
        catch (...) {
            test_offset = 0.00;
        }
    }

    //
    fee_data sell_fee{0.12, 0.0};
    fee_data buy_fee{0.0, 0.01};
    //
    if (ui.enable_arbitrage->isChecked())
    {
        xrpl_network_->get_orderbook().compute_arbitrage(bitstamp_network_->get_orderbook(),
            budget, buy_fee, sell_fee, test_offset, arbitrage_string);

        if (arbitrage_string.size()>0) {
            QString arb_string = QString::fromStdString(arbitrage_string);
            ui.arbitrage_orders->setPlainText(arb_string);
        }
        else {
            ui.arbitrage_orders->setPlainText("");
        }
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::transaction_event()
{
    main_dbg<5>.debug("transaction_event : check balances");
    update_account_balances();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::restart_candlestick_timer_event()
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
        main_dbg<5>.debug("Overriding: candlestick timer", delay_seconds, "seconds");
        timer_->start(delay_seconds*1000);
    }
    else {
        main_dbg<5>.debug("restarting candlestick timer", delay_seconds, "seconds");
        timer_->start(delay_seconds*1000);
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::candlestick_timer_event()
{
    QString now(QDateTime::currentDateTime().toString("dd.MM.yy hh:mm:ss"));
    main_dbg<5>.debug("candlestick_timer_event : " + now.toStdString());
    if (!candlestick_update_active_) {
        update_candlestick_data();
    }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::orderbook_text_update()
{
    QString datastring = QString::fromStdString(bitstamp_network_->get_orderbook().order_text);
    ui.order_book_bitstamp->setPlainText(datastring);

    datastring = QString::fromStdString(xrpl_network_->get_orderbook().order_text);
    ui.order_book_xrpl->setPlainText(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::display_offers()
{
    // Delete previous space in offer window
    for (int i = 0; i < orders_scrollwidget->widget()->layout()->count(); ++i) {
        QLayoutItem *layoutItem = orders_scrollwidget->widget()->layout()->itemAt(i);
        if (layoutItem->spacerItem()) {
            orders_scrollwidget->widget()->layout()->removeItem(layoutItem);
            delete layoutItem;
            --i;
        }
    }

    app_settings* app_ini = global_settings();
    // for each wallet on each network
    for (auto network : app_ini->networks_) {
        for (auto w : network->wallets()) {
            check_trades_dialog::create_trade_widgets(orders_scrollwidget->widget(), w->name_, w->offers_);
        }
    }
    // absorb any extra space in the parent by adding a spacer
    orders_scrollwidget->widget()->layout()->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

// ----------------------------------------------------------------------------
// Load/Save of ini configuration/settings for main Qt application
// ----------------------------------------------------------------------------
void GroxMainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowSettings();
    saveTrustlines();
    saveConnectionSetups();
    QMainWindow::closeEvent(event);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::showEvent(QShowEvent *event )
{
    QMainWindow::showEvent(event);
    // load settings on startup window display only
    static bool only_once = true;
    if (only_once) {
        loadWindowSettings();
        only_once = false;
    }
    // start timer that will fetch latest candlestick data
    timer_->start(2000);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveTrustlines()
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start GroxMainWindow section
    settings.beginGroup("Trustlines");
    int index = 0;
    for (auto const &t : currency::trustlines) {
        std::string key = std::to_string(index++) + '_' + hex_to_currency(t.code_);
        settings.setValue(key.c_str(), t.issuer_.c_str());
    }
    settings.endGroup();
    main_dbg<0>.debug(str<>("Trustlines saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadTrustlines()
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start section
    settings.beginGroup("Trustlines");
    QStringList childKeys = settings.childKeys();
    for (auto const &k : childKeys) {
        std::string key = k.toStdString();
        auto delim = key.find('_');
        std::string code = key.substr(delim+1, key.back());
        std::string issuer = settings.value(k).toString().toStdString();
        currency::trustlines.push_back({issuer, currency_to_hex(code)});
    }
    settings.endGroup();
    main_dbg<0>.debug(str<>("Trustlines loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveConnectionSetups()
{
    // connection_widget_;
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start section
    settings.beginGroup("Streams");
    for (const auto &e : exchange_list_) {
        std::string prefix(e->name());
        auto streams = e->websocket_streams();
        for (const auto &s : streams) {
            std::string key = prefix + "_" + stream_text(s);
            settings.setValue(key.c_str(), e->websocket_enabled(s));
        }
    }

    settings.endGroup();
    main_dbg<0>.debug(str<>("Connections saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadConnectionSetups()
{
    // connection_widget_;
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start section
    settings.beginGroup("Streams");
    for (const auto &e : exchange_list_) {
        std::string prefix(e->name());
        auto streams = e->websocket_streams();
        for (const auto &s : streams) {
            std::string key = prefix + "_" + stream_text(s);
            bool enabled = settings.value(key.c_str()).toBool();
            e->websocket_enable(s, io_contexts_, enabled);
        }
    }

    settings.endGroup();
    main_dbg<0>.debug(str<>("Connections loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveWindowSettings()
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start section
    settings.beginGroup(objectName());
#ifdef workaround
    settings.setValue("geometry", QVariant(geometry()));
    settings.setValue("windowState",saveState());
#else
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
#endif

    // save splitter state
    QByteArray state = ui.graph_splitter->saveState();
    settings.setValue("graphSplitter", state.toBase64());

    // save active tab
    // QString currentTabName = ui.main_tabbook->currentWidget()->objectName();
    settings.setValue("mainwindowTabIndex", ui.main_tabbook->currentIndex());

    settings.endGroup();
    main_dbg<0>.debug(str<>("Settings saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadWindowSettings()
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start GroxMainWindow section
    settings.beginGroup(objectName());

#ifdef workaround
    if(settings.contains("geometry"))
        setGeometry(settings.value("geometry").value<QRect>());
    restoreState(settings.value("windowState").toByteArray());
#else
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
#endif
    // load splitter state
    ui.graph_splitter->restoreState(QByteArray::fromBase64(settings.value("graphSplitter").toByteArray()));

    // load active tab
    int mainwindowTabIndex = settings.value("mainwindowTabIndex").toInt();
    ui.main_tabbook->setCurrentIndex(mainwindowTabIndex);

    settings.endGroup();
    main_dbg<0>.debug(str<>("Settings loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::stream_process(const QwtOHLCSample &ohlc)
{
    main_dbg<0>.debug(str<>("New data"), msecs_unix_to_calendar_time(ohlc.time), ohlc);
//    df_.process(ohlc);
}

// ----------------------------------------------------------------------------
QColor colours[10] = {QColor("cyan"), QColor("magenta"), QColor("red"),
                      QColor("darkRed"), QColor("darkCyan"), QColor("darkMagenta"),
                      QColor("green"), QColor("darkGreen"), QColor("yellow"),
                      QColor("blue")};

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_filter()
{
    static int col = 0;

    trade_algorithm ta_dialog = trade_algorithm();
    auto result = ta_dialog.exec();
    if (result == QDialog::Rejected)
        return;
    if (result != QDialog::Accepted) {
        col = 0;
        crypto_price_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);

        filters_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
        filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

        assets_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
        assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);
        return;
    }

    bool all_resolutions = false;
    int algorithm = ta_dialog.algorithm();

    std::vector<candle_res> resolutions = ohlc_chart_data::available_resolutions();
    candle_res base_resolution = ohlc_chart_data::minute;
    // get the highest resolution used by the algorithm
    if (!all_resolutions) {
        resolutions.clear();
        base_resolution = ta_dialog.resolution(0);
        resolutions.push_back(base_resolution);
    }
    // for each dataset required, get the GCD to use as a base resolution
    for (int i=1; i<ta_dialog.num_datasets(); ++i) {
        candle_res res = ta_dialog.resolution(i);
        base_resolution = ohlc_chart_data::gcd(base_resolution, res);
    }

//    base_resolution = ohlc_chart_data::minute;

    auto dataset = hdf5_ohlc_.get_dataset(base_resolution);
    auto data = dataset->ohlc_samples_;

    // NB. Inputs need to be used by reference, with the original object
    // kept alive so that they can be updated continuously.
    // Other filter objects are copied during pipeline construction

    // initialize an ohlc input object with the first dataset value
    input_value<QwtOHLCSample> ohlc_in(data->data().front());
    pipeline::input<const QwtOHLCSample&> ohlc_input = std::ref(ohlc_in);

    // initialize a time input object
    input_value<double> time_in(data->data().front().time);
    pipeline::input<double> time_input = std::ref(time_in);

    struct funds {
        double xrp;
        double usd;
    };

    std::vector<price_type> price_pipelines;
    std::vector<price_type> filter_pipelines;
    std::vector<event_type> event_pipelines;
    std::vector<funds> funding;

    for (const candle_res &res : resolutions) {
        int N = res.res_ / base_resolution;
        if (algorithm==0)
            make_heikin_ashi_pipeline(ohlc_input, time_input, res, event_pipelines, price_pipelines);
        else if (algorithm==1)
            make_moving_average_gradient(ohlc_input, time_input, N, event_pipelines, price_pipelines);
        else if (algorithm==2) {
            candle_res res2 = ta_dialog.resolution(1);
            int M = res2.res_ / base_resolution;
            make_moving_average_cross(ohlc_input, time_input, N, M, event_pipelines, price_pipelines);
        }
        else if (algorithm==3) {
            make_MACD(ohlc_input, time_input, 12, 26, 12*9, event_pipelines, price_pipelines, filter_pipelines);
        }
        funding.push_back({50000,0});
    }

    using plot_array = QVector<QPointF>;
    plot_array buys, sells, assets;
    std::vector<plot_array> priceplots;
    std::vector<plot_array> filterplots;
    buys.reserve(5000);
    sells.reserve(5000);
    assets.reserve(5000);
    for (const auto &p : price_pipelines) {
        plot_array &temp = priceplots.emplace_back();
        temp.reserve(5000);
    }
    for (const auto &p : filter_pipelines) {
        plot_array &temp = filterplots.emplace_back();
        temp.reserve(5000);
    }

    auto start = QDateTime( QDate(2020, 10, 1), QTime(0,0,0), QTimeZone::utc());
    //start = ui.repair_date->dateTime();
    //
    double msecs = start.toMSecsSinceEpoch();
    uint64_t start_index = data->sample_index(msecs);
    uint64_t i = 0;
    //
    double fee_estimate = 0.998;

    for (auto const &ohlc : data->data()) {
        if (i++<start_index)
            continue;

        ohlc_in.set(ohlc);
        time_in.set(ohlc.time + base_resolution.res_ /*+ ohlc_chart_data::minute*/);

        for (uint i=0; i<price_pipelines.size(); ++i) {
            auto &pipe = price_pipelines[i];
            auto &data = priceplots[i];
            double p = pipe.operator()();
            QPointF trade2(ohlc.time, p);
            data.push_back(trade2);
        }

        for (uint i=0; i<filter_pipelines.size(); ++i) {
            auto &pipe = filter_pipelines[i];
            auto &data = filterplots[i];
            double p = pipe.operator()();
            QPointF trade2(ohlc.time, p*10);
            data.push_back(trade2);
        }

        auto &pipe = event_pipelines[0];
        trade_event e = pipe.operator()();
        if (e.type_ == buy_sell_type::buy_event) {
            if (funding[0].usd>0) {
                double p = hdf5_ohlc_.get_estimated_buy_price(funding[0].usd, e.time_, 2.0);
                if (p==0) break;

                // plot buy price
                QPointF trade(e.time_, p);
                buys.push_back(trade);

                funding[0].xrp = fee_estimate*funding[0].usd/p;
                funding[0].usd = 0;
                std::cout << "Buy  :" << msecs_unix_to_calendar_time(e.time_) << " "
                          << "Res " << str<6>(base_resolution.name_)
                          << "xrp (" << fp<2,11>(funding[0].xrp) << ") "
                          << "usd (" << fp<2,11>(funding[0].usd) << ") "
                          << "\n";
                // plot current assets
                QPointF trade2(e.time_, funding[0].xrp);
                assets.push_back(trade2);
            }
        }
        else if (e.type_ == buy_sell_type::sell_event) {
            if (funding[0].xrp>0) {
                double p = hdf5_ohlc_.get_estimated_sell_price(funding[0].xrp, e.time_, 2.0);
                if (p==0) break;

                // plot sell price
                QPointF trade2(e.time_, p);
                sells.push_back(trade2);

                // plot current assets (before resetting xrp to zero)
                QPointF trade3(e.time_, funding[0].xrp);
                assets.push_back(trade3);

                funding[0].usd = fee_estimate*funding[0].xrp*p;
                funding[0].xrp = 0;
                std::cout << "Sell :" << msecs_unix_to_calendar_time(e.time_) << " "
                          << "Res " << str<6>(base_resolution.name_)
                          << "xrp (" << fp<2,11>(funding[0].xrp) << ") "
                          << "usd (" << fp<2,11>(funding[0].usd) << ") "
                          << "\n";
            }
        }
//            static auto algo_deb =
//                mainwin_debug.make_timer(60, str<>("Algorithm"));

//            mainwin_debug.timed(algo_deb, "time",
//                str<20>(msecs_unix_to_calendar_time(e.time_).c_str())
//                , lambda(
//                    [&](){
//                        int res_i = 0;
//                        std::stringstream temp;
//                        temp << "\n";
//                        for (const candle_res &res : resolutions) {
//                            temp << "res "  << str<5>(res.name_), ""
//                                 << "xrp (" << fp<2,9>(funding[0].xrp) << ") "
//                                 << "usd (" << fp<2,9>(funding[0].usd) << ") "
//                                 << "\n";
//                            res_i++;
//                        }
//                        return temp.str();
//                    })
//            );
    }

    for (uint i=0; i<price_pipelines.size(); ++i) {
        auto &data = priceplots[i];
        crypto_price_plot_->add_price_curve("MA", data, colours[col++]);
    }
    crypto_price_plot_->add_buy_sell_curve("Buy",  buys,  Qt::green);
    crypto_price_plot_->add_buy_sell_curve("Sell", sells, Qt::red);
    //
    assets_plot_->add_asset_curve("Value", assets, Qt::red);
    for (uint i=0; i<filter_pipelines.size(); ++i) {
        auto &data = filterplots[i];
        filters_plot_->add_asset_curve("MA", data, colours[col++]);
    }

    int res_i = 0;
    for (const candle_res &res : resolutions) {
        std::cout << "res : " << res.name_ << " "
                  << "xrp (" << fp<2,9>(funding[0].xrp) << ") "
                  << "usd (" << fp<2,9>(funding[0].usd) << ") "
                  << "\n";
        res_i++;
    }
}
