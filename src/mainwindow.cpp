#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeySequence>
#include <QShortcut>
#include <QMessageBox>
#include <QDockWidget>
#include <QScrollBar>
//
#include <QwtScaleMap>
#include <QwtScaleDiv>
//
#include <filesystem>
//
#include <boost/format.hpp>
//
#include "mainwindow.hpp"
#include "src/widgets/password_dialog.hpp"
#include "src/widgets/wallet_widget.hpp"
#include "src/widgets/currency_widget.hpp"
#include "src/widgets/trade_widget.hpp"
#include "src/widgets/check_trades_dialog.hpp"
//
#include "src/network/evp-encrypt.hpp"
#include "src/network/https-async.hpp"
//
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
//
#include "ohlc.hpp"
#include "settings.hpp"
//
#include <iostream>
#include <iomanip>
#include <ctime>
//
#ifndef DEBUG_ONLY
# define DEBUG_ONLY(x)
# define DEBUG_ALWAYS(x) { \
    std::stringstream temp; temp << x; \
    std::cout << temp.str() << std::endl; }
#endif

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

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
    CombinedPriceVolumeCharts_ = new CombinedPriceVolumeCharts(this, &hdf5_ohlc_);
    cryptoPricePlot_ = CombinedPriceVolumeCharts_->get_CryptoPricePlot();
    ui.candlestick_layout->addWidget(CombinedPriceVolumeCharts_, 30);

    // ----------------------------------
    // Create orderbook plot
    //
    obp_ = new OrderBookPlot(); // std::make_shared<OrderBookPlot>();
    obp_->setMinimumSize(384,256);
    //obp_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.order_plot_layout->addWidget(obp_/*.get()*/, 0);


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

    // timer will fire once each time it is reset
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);

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
    if (!hdf5_ohlc_.empty()) {
        cryptoPricePlot_->update_data_array(&hdf5_ohlc_);
        // start by displaying one day of data
        graph_rescale(0);
    }

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
    accounts_dock->setFeatures(QDockWidget::AllDockWidgetFeatures);
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
    orders_dock->setFeatures(QDockWidget::AllDockWidgetFeatures);
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

    // ----------------------------------
    // Subscribe to xrpl events
    //
    xrpl_network_->subscribe_orderbook(io_contexts);
    xrpl_network_->subscribe_accounts(io_contexts);
    xrpl_testnet_->subscribe_accounts(io_contexts);

    // Run the I/O service on some threads.
    for (int i=0; i<2; ++i) {
        websocket_thread = std::thread([&]() {
            DEBUG_ONLY("io_contexts run : thread " << std::this_thread::get_id());
            // The call will return when the socket is closed.
            io_contexts.ioc.run();
        });
        websocket_thread.detach();
    }
    update_account_balances();

    //
    // Resize order book to fit monospace text (add 1 chars - scrollbars/etc)
    //
    QString txt = "X";
    int char_size = QFontMetrics(ui.order_book_xrpl->font()).horizontalAdvance(txt);
    int calcWidth = char_size*85 + 8;
    //std::cout << "width " << ui.order_book_xrpl->verticalScrollBar()->geometry().width() << std::endl;
    ui.order_book_xrpl->setMinimumWidth(calcWidth);
    //ui.order_book_xrpl->setMaximumWidth(calcWidth);
    ui.order_book_bitstamp->setMinimumWidth(calcWidth);
    //ui.order_book_bitstamp->setMaximumWidth(calcWidth);
    //
    calcWidth = char_size*140 + 8;
    ui.arbitrage_orders->setMinimumWidth(calcWidth);
    //ui.arbitrage_orders->setMaximumWidth(calcWidth);
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
    delete timer_;
    delete CombinedPriceVolumeCharts_;
    delete obp_;
}

// ----------------------------------------------------------------------------
void GroxMainWindow::appExitCleanupHandler()
{
    qDebug() << "Main Window: appExitCleanupHandler()";
    // call clean up handlers of any components/widgets
    // block here to prevent access of temp buffers that are deleted
    // by the program/qt/etc
    //
    bitstamp_network_->disconnect();
    bitstamp_network_.reset();
    //
    xrpl_network_->disconnect();
    xrpl_network_.reset();
    //
    xrpl_testnet_->disconnect();
    xrpl_testnet_.reset();
    qDebug() << "websockets: shutdown complete";

    // stop boost::asio io_service
    io_contexts.ioc.stop();
    qDebug() << "boost::asio: shutdown complete";
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
            std::array<std::string, 5> strings{
                bitstamp_network_->account().API_user, bitstamp_network_->account().API_key,
                bitstamp_network_->account().API_secret, std::to_string(bitstamp_network_->account().tag_),
                bitstamp_network_->account().public_};

            // iterate over wallets to commvert type from basic pointers
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

    // button-click : main connect networks start button
    connect(ui.connect_button, SIGNAL(clicked()), this, SLOT(start_websocket()));

    // button-click : fetch latest account balance data
    connect(ui.account_update, SIGNAL(clicked()), this, SLOT(update_account_balances()));

    // when new candlestick data is ready, redo main graph
    connect(this, SIGNAL(new_ohlc_data_ui()), this, SLOT(new_ohlc_data()));

    // ---------------------------------------------------------------------
    // signals emitted from networking thread completion handlers should use
    // Qt::QueuedConnection to ensure they transfer to Qt main thread
    // ---------------------------------------------------------------------

    // used to fetch account balances after N seconds
    connect(timer_, SIGNAL(timeout()), this, SLOT(on_timer()));

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

    connect(bitstamp_network_.get(), &bitstamp_network::new_trade_data_ui, this, [](live_trades t) {
        std::cout << "Signal captured : " << t.amount<< std::endl;
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
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::graph_rescale(int range)
{
    auto last_time = hdf5_ohlc_.get_last_sample_time();
    auto day = 60.0*60.0*24.0*1000.0;
    auto hour = 60.0*60.0*1000.0;
    double t1=0, t2 = last_time;
    double stepSize = 0;
    if (range==-2) {
        t1 = last_time - 0.25*day;
        stepSize = hour;
    }
    else if (range==-1) {
        t1 = last_time - 0.5*day;
        stepSize = 2*hour;
    }
    else if (range==0) {
        t1 = last_time - 1.0*day;
        stepSize = 4*hour;
    }
    else if (range==1) {
        t1 = last_time - 7*day;
        stepSize = day;
    }
    else if (range==2) {
        t1 = last_time - 31*day;
        stepSize = 7*day;
    }
    else if (range==3) {
        t1 = last_time - 365*day;
        stepSize = 31*day;
    }
    // special case, to extend current view with new data
    else if (range==100) {
        t1 = last_time - 365*day;
    }
    else {
        t1 = hdf5_ohlc_.get_first_sample_time();
    }
    auto minmax = hdf5_ohlc_.get_min_max_window(t1, t2, 0.05);
    cryptoPricePlot_->setAxisScale(QwtAxis::XBottom, t1, t2, stepSize);
    cryptoPricePlot_->setAxisScale(QwtAxis::YLeft, minmax.minval_, minmax.maxval_);
    cryptoPricePlot_->adjust_candle_size();
    cryptoPricePlot_->replot();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::new_ohlc_data()
{
    // don't change axes, just update data series and replot
    cryptoPricePlot_->update_data_array(&hdf5_ohlc_);
    cryptoPricePlot_->replot();
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
        qDebug() << "Yes was clicked";
        app_settings* app_ini = global_settings();

        std::uint32_t tag = bitstamp_network_->account().tag_;
//        bool test = make_xrp_payment(ripple::KeyType::secp256k1,
//            app_ini->xrpl_wallets[app_ini->active_wallet].private_,
//            app_ini->xrpl_wallets[app_ini->active_wallet].public_,
//            bitstamp_network_->account().public_, tag, 10);

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
            // convert 1 minute candle OHLC data to msecs
            temp.timestamp *= 1000;
            new_ohlc_samples.push_back(QwtOHLCSample(
                temp.timestamp, temp.open, temp.high, temp.low, temp.close));
            new_ohlc_volumes.push_back(temp.volume);
        }
        DEBUG_ALWAYS("Received " << ohlc_strings.size()
                  << " new OHLC samples");
        hdf5_ohlc_.merge_data(new_ohlc_samples, new_ohlc_volumes);
        emit new_ohlc_data_ui();

        // what is the last sample we currently have
        auto last_time = hdf5_ohlc_.get_last_sample_time();
        auto end_t = static_cast<uint64_t>(last_time/1000);
        std::time_t t(end_t);
        std::tm tm = *std::localtime(&t);
        std::cout << "Data merged up to " << std::put_time(&tm, "%F %T") << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "JSON error decoding OHLC data: " << e.what() << "\n"
                  << data << std::endl << std::endl;
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
    DEBUG_ALWAYS("Updating accounts");
    //
    bitstamp_network_->get_account_info();
    bitstamp_network_->get_open_orders();
    //
    xrpl_network_->get_all_account_balances();
    xrpl_network_->get_all_account_infos();
    xrpl_network_->get_all_account_orders();
    //
    if (1) {
        xrpl_testnet_->get_all_account_balances();
        xrpl_testnet_->get_all_account_infos();
    }
}

// ----------------------------------------------------------------------------
static std::mutex time_mutex;
std::string unix_time_to_calendar_time(uint64_t unixtime)
{
    // we use a mutex here, because std::localtime isn't threadsafe
    std::lock_guard<std::mutex> lock(time_mutex);
    std::time_t t(unixtime);
    std::tm tm = *std::localtime(&t);
    std::stringstream temp;
    temp << std::put_time(&tm, "%F %T");
    return temp.str();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::update_candlestick_data()
{
    uint64_t start_t = 0;
    // what is the last sample we currently have
    if (!hdf5_ohlc_.empty())
    {
        // convert msecs back to secs
        start_t = static_cast<uint64_t>(hdf5_ohlc_.get_last_sample_time()/1000);
        DEBUG_ALWAYS("Data present up until " << unix_time_to_calendar_time(start_t));
        start_t += 60;    // next sample is 60s after last
    }
    std::cout << "Requesting candlestick data from " << unix_time_to_calendar_time(start_t) << std::endl;

    // @TODO add futures here to make dependency chain simpler
    bitstamp_network_->request_new_candlestick_data(start_t, [this, start_t](auto& ctx, bool more) {
        std::cout << "Received candlestick data from " << unix_time_to_calendar_time(start_t) << std::endl;
        this->receive_ohlc_data(std::move(ctx.res.body()));
        if (more) {
            this->update_candlestick_data();
        }
    });
}

// ----------------------------------------------------------------------------
void GroxMainWindow::start_websocket()
{
    bitstamp_network_->connect(io_contexts);
    update_candlestick_data();
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
    DEBUG_ALWAYS("transaction_event : check balances");
    update_account_balances();
    // set timer for 5 seconds to check again after next ledger close
    timer_->start(5000);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::on_timer()
{
    DEBUG_ALWAYS("on_timer : check balances");
    update_account_balances();
//    timer_->start(5000);
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
void GroxMainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowSettings();
    QMainWindow::closeEvent(event);
}

void GroxMainWindow::showEvent(QShowEvent *event )
{
    loadWindowSettings();
    QMainWindow::showEvent(event);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveWindowSettings()
{
    app_settings* app_ini = global_settings();
    QSettings settings(app_ini->iniFileName, QSettings::IniFormat);
    // Start GroxMainWindow section
    settings.beginGroup(objectName());
#ifdef workaround
    settings.setValue("geometry", QVariant(geometry()));
    settings.setValue("windowState",saveState());
#else
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
#endif
    settings.endGroup();
    qDebug() << "Settings saved under:" << settings.fileName();
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
    settings.endGroup();
    qDebug() << "Settings loaded from:" << settings.fileName();
}
