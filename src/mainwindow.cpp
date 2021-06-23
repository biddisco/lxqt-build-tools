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
#include <filesystem>
//
#include <boost/format.hpp>
//
#include "mainwindow.hpp"
#include "password_dialog.hpp"
#include "wallet_widget.hpp"
#include "currency_widget.hpp"
#include "trade_widget.hpp"
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
#include "hdf5.h"
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
    CombinedPriceVolumeCharts_ = new CombinedPriceVolumeCharts(this);
    priceAndPatternPlot_ = CombinedPriceVolumeCharts_->priceAndPatternPlot();
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
    bitstamp_network_ = std::dynamic_pointer_cast<bitstamp_network>(bitstamp_network::get_instance());
    bitstamp_network_->set_plot(obp_);

    // ----------------------------------
    // create xrp network interfaces
    // we do not plot the xrp testnet orderbook
    //
    xrpl_network_ = std::dynamic_pointer_cast<xrpl_network>(xrpl_network::get_xrpl_instance());
    xrpl_testnet_ = std::dynamic_pointer_cast<xrpl_network>(xrpl_network::get_xrpltestnet_instance());
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
    // Load existing candlestick data
    read_hdf5();

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
    QFrame *orders_frame = new QFrame(orders_scrollwidget);
    orders_frame->setLayout(new QVBoxLayout());
    orders_scrollwidget->setWidget(orders_frame);
    orders_dock = std::make_shared<QDockWidget>("Orders", this);
    orders_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    orders_dock->setFeatures(QDockWidget::AllDockWidgetFeatures);
    orders_dock->setObjectName("OrdersDock");
    orders_dock->setWidget(orders_scrollwidget);
    addDockWidget(Qt::RightDockWidgetArea, orders_dock.get());

    //
//    QFrame *frame = new QFrame(this);
//    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
//    accounts_scrollwidget->setWidget(frame);
    //
//    QVBoxLayout *vbox = new QVBoxLayout();
//    frame->setLayout(vbox);
    //
    app_settings* app_ini = global_settings();
    {
        app_ini->bitstamp.widget_ = new wallet_widget(this);
        app_ini->bitstamp.widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        app_ini->bitstamp.widget_->set_data(app_ini->bitstamp);
        accounts_frame->layout()->addWidget(app_ini->bitstamp.widget_);
    }

    ranges::for_each(app_ini->xrpl_wallets, [&](auto &w) {
        // create a gui widget for the wallet
        w.widget_ = new wallet_widget(this);
        w.widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        w.widget_->set_data(w);
        accounts_frame->layout()->addWidget(w.widget_);
        // update wallet combo with name
        ui.all_acct_combo->addItem(QString(w.name_.c_str()));
        // updte networks with monitoried wallets
        if (w.testnet_) xrpl_testnet_->add_wallet(w);
        else xrpl_network_->add_wallet(w);
    });
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

    // when trades are queried/changed, we must update the GUI
    connect(bitstamp_network_.get(), SIGNAL(user_trades_updated(QString)),
            this, SLOT(user_trades_update(QString)), Qt::QueuedConnection);

    // when a transaction takes place we might need to update wallet/records
    connect(bitstamp_network_.get(), SIGNAL(transaction_event()),
            this, SLOT(transaction_event()), Qt::QueuedConnection);

    connect(bitstamp_network_.get(), &bitstamp_network::widget_update, this, []() {
        app_settings* app_ini = global_settings();
        app_ini->bitstamp.widget_->set_data(app_ini->bitstamp);
    }, Qt::QueuedConnection);

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
        DEBUG_ALWAYS("Received " << ohlc_strings.size()
                  << " new OHLC samples");
        merge_data(new_ohlc_samples, new_ohlc_volumes);
        // what is the last sample we currently have
        auto end_t = static_cast<uint64_t>(ohlc_samples.back().time);
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
void GroxMainWindow::new_ohlc_data()
{
    if (!ohlc_samples.empty())
        priceAndPatternPlot_->set_OHLC_data(ohlc_samples);
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
    bitstamp_network_->update_account_info();
    bitstamp_network_->get_open_orders();
    //
    xrpl_network_->get_all_account_balances();
    xrpl_network_->get_all_account_infos();
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
    if (ohlc_samples.size() > 0)
    {
        start_t = static_cast<uint64_t>(ohlc_samples.back().time);
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
    timer_->start(5000);
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
void GroxMainWindow::user_trades_update(QString data)
{
    DEBUG_ALWAYS(data.toStdString());
    // Wipe the old order widgets
    QWidget *old_frame = orders_scrollwidget->findChild<QWidget*>("Bitstamp");
    if (old_frame) {
        orders_scrollwidget->widget()->layout()->removeWidget(old_frame);
        delete old_frame;
    }
    //
    QFrame *frame = new QFrame(this);
    frame->setObjectName("Bitstamp");
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    orders_scrollwidget->widget()->layout()->addWidget(frame);
    //
    QVBoxLayout *vbox = new QVBoxLayout();
    frame->setLayout(vbox);
    //
    std::string temp = data.toStdString();
    nlohmann::json jdata = json::parse(temp);
    for (auto& [key, val] : jdata.items())
    {
        std::string cs = val["currency_pair"];
        std::size_t pos = cs.find("/");
        std::string c1 = cs.substr(0,pos);
        std::string c2 = cs.substr(pos+1);

        double amount = std::stod(val["amount"].get< std::string >());
        double price = std::stod(val["price"].get< std::string >());
        trade_data t{
            bitstamp_network_,
            (val["type"] == "1") ? 1 : 0,
            get_currency_type(c1,""),
            get_currency_type(c2,""),
            amount,
            price,
            amount*price,
            std::stoull(val["id"].get< std::string >()),
            val["datetime"]
        };

        // create a gui widget for the order
        auto widget_ = new trade_widget(this);
        widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        widget_->set_data(t);
        vbox->addWidget(widget_);
    }

    vbox->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

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
