// STL
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
// Qt
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QKeySequence>
#ifdef QT6
# include <QKeyCombination>
#endif
#include <QCheckBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QListView>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardItemModel>
// Qwt
#include <QwtAxis>
#include <QwtScaleDraw>
#include <QwtScaleEngine>
#include "data/ohlctv_sample.hpp"
// Grox
#include "data/ohlc_heikin_ashi.hpp"
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
#include "io/hdf5_ohlc_manager.hpp"
#include "mainwindow.hpp"
#include "network/evp-encrypt.hpp"
#include "network/https-async.hpp"
#include "util/datetime_utils.hpp"
#include "widgets/check_trades_dialog.hpp"
#include "widgets/connection_widget.hpp"
#include "widgets/currency_widget.hpp"
#include "widgets/password_dialog.hpp"
#include "widgets/trade_widget.hpp"
#include "widgets/wallet_widget.hpp"
// Qt Advanced Docking System
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "DockManager.h"
#include "FloatingDockContainer.h"

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> main_dbg("Main-win");

using namespace ads;

// ----------------------------------------------------------------------------
// we do not use a unique_ptr with automatic delete because ...
// QObject::killTimer: Timers cannot be stopped from another thread
QTimer* app_settings::get_global_clock_timer()
{
  static QTimer* timer_ = nullptr;
  if (!timer_)
  {
    timer_ = new QTimer(nullptr);
    timer_->start(1000);
  }
  return timer_;
}

void app_settings::delete_global_clock_timer(QTimer* timer_)
{
  delete timer_;
}

// ----------------------------------------------------------------------------
GroxMainWindow::GroxMainWindow(QWidget* parent)
  : QMainWindow(parent)
  , net_layout_(nullptr)
  , perspectives_menu_(nullptr)
  , dark_mode_(0)
{
  ui.setupUi(this);

  // ----------------------------------
  // global persistent settings
  global_settings.dockwindows_menu_ = nullptr;

  // ----------------------------------
  global_settings.data_manager_ =
    std::dynamic_pointer_cast<abstract_dataset_manager>(std::make_shared<hdf5_ohlc_manager>());
  global_settings.data_manager_->init(global_settings.appDataLocation, global_settings.hdfFileName);

  // ----------------------------------
  // Create Dock manager and set default flags
  // Because the parent parameter is a QMainWindow the dock manager registers
  // itself as the central widget as such the ui must be set up first.
  CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
  CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
  CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
  global_settings.dock_manager_ = std::make_shared<CDockManager>(this);

  // ----------------------------------
  // Setup a menu to allow dockwindow control
  createPerspectives_Ui();

  // ----------------------------------
  // create bitstamp exchange interface
  bitstamp_network_ = bitstamp_network::get_bitstamp_instance();

  // ----------------------------------
  // create xrp network interfaces
  // we do not plot the xrp testnet orderbook
  xrpl_network_ = xrpl_network::get_xrpl_instance(false);
  xrpl_testnet_ = xrpl_network::get_xrpl_instance(true);
  //xrpl_network_->set_plot(obp_);

  exchange_list_.push_back(bitstamp_network_);
  exchange_list_.push_back(xrpl_network_);
  exchange_list_.push_back(xrpl_testnet_);

  // ----------------------------------
  // Create dockwidget for network connections
  QFrame* netbox = new QFrame(this);
  net_layout_ = new QVBoxLayout();
  netbox->setLayout(net_layout_);
  //
  CDockWidget* NetworkDockWidget = new CDockWidget("Networks");
  NetworkDockWidget->setWidget(netbox);
  NetworkDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  auto RightArea = global_settings.dock_manager_->addDockWidget(
    DockWidgetArea::RightDockWidgetArea, NetworkDockWidget);
  global_settings.dockwindows_menu_->addAction(NetworkDockWidget->toggleViewAction());

  // ----------------------------------
  // Create dockwidget for algorithmic trading
  QWidget* algowidget_ = new QWidget(this);
  algo_form_ = new Ui::TabbedForm();
  algo_form_->setupUi(algowidget_);

  CDockWidget* AlgorithmsDockWidget = new CDockWidget("Algorithms");
  AlgorithmsDockWidget->setWidget(algowidget_);
  AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(
    DockWidgetArea::RightDockWidgetArea, AlgorithmsDockWidget, RightArea, 1);
  global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

  // ----------------------------------
  // create a dock widget to hold accounts/wallets
  accounts_frame_ = new QFrame();
  accounts_frame_->setLayout(new QVBoxLayout());
  //
  CDockWidget* AccountsDockWidget = new CDockWidget("Accounts");
  AccountsDockWidget->setWidget(accounts_frame_);
  AccountsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(
    DockWidgetArea::RightDockWidgetArea, AccountsDockWidget, RightArea, 2);
  global_settings.dockwindows_menu_->addAction(AccountsDockWidget->toggleViewAction());

  // ----------------------------------
  // create a dock widget to hold open orders
  orders_frame_ = new QFrame();
  orders_frame_->setLayout(new QVBoxLayout());
  //
  CDockWidget* OrdersDockWidget = new CDockWidget("Trades");
  OrdersDockWidget->setWidget(orders_frame_);
  OrdersDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  global_settings.dock_manager_->addDockWidget(
    DockWidgetArea::RightDockWidgetArea, OrdersDockWidget, RightArea, 3);
  global_settings.dockwindows_menu_->addAction(OrdersDockWidget->toggleViewAction());

  // for each wallet on each network
  for (auto network : global_settings.networks_)
  {
    for (auto w : network->wallets())
    {
      // create a gui widget for the wallet
      w->widget_ = new wallet_widget(this);
      w->widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      if (network->name() == "Bitstamp")
        w->widget_->set_data(*static_cast<bitstamp_account*>(w));
      if (network->name() == "XRPL")
        w->widget_->set_data(*static_cast<ledger_wallet*>(w));
      accounts_frame_->layout()->addWidget(w->widget_);
      // update wallet combo with name
      algo_form_->all_acct_combo->addItem(QString(w->name_.c_str()));
    }
  }
  accounts_frame_->layout()->addItem(
    new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred));

  // ----------------------------------
  // setup Qt actions/connections between controls
  connect_gui_controls();

  // ----------------------------------
  // load in the list of trustlines that we know about
  loadTrustlines();

  // ----------------------------------
  // start asio threads
  start_io_threads(2);

  // ----------------------------------
  // initialize networks / start websocket connections etc
  main_dbg<0>.debug("Init bitstamp");
  bitstamp_network_->initialize();
  //
  main_dbg<0>.debug("Init xrpl mainnet");
  xrpl_network_->initialize();
  //
  main_dbg<0>.debug("Init xrpl testnet");
  xrpl_testnet_->initialize();

  // ----------------------------------
  // setup connections tab
  loadConnectionSetups();

  // @TODO get rid of this
  // ----------------------------------
  // Resize order book to fit monospace text (add 1 chars - scrollbars/etc)
  //
  QString txt = "X";
  int char_size = QFontMetrics(algo_form_->order_book_xrpl->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  //std::cout << "width", algo_form_->order_book_xrpl->verticalScrollBar()->geometry().width() << std::endl;
  algo_form_->order_book_xrpl->setMinimumWidth(calcWidth);
  //algo_form_->order_book_xrpl->setMaximumWidth(calcWidth);
  //algo_form_->order_book_bitstamp->setMaximumWidth(calcWidth);
  //
  calcWidth = char_size * 140 + 8;
  algo_form_->arbitrage_orders->setMinimumWidth(calcWidth);
  //algo_form_->arbitrage_orders->setMaximumWidth(calcWidth);

  // ----------------------------------
  // just an experiment to display an image
  // scale pixmap to fit in label's size and keep ratio of pixmap
  QPixmap pix(":/images/xrp.jpg");
  // pix = pix.scaled(algo_form_->image_label->size(), Qt::KeepAspectRatio);
  // algo_form_->image_label->setPixmap(pix);
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
  delete qs_shutdown_;
  delete qs_darkmode_;
  //
  global_settings.delete_global_clock_timer(global_settings.get_global_clock_timer());

  // release dockmanager
  global_settings.dock_manager_.reset();
  // release all networks
  for (auto& n : global_settings.networks_)
    n.reset();
  // release datamanager
  global_settings.data_manager_.reset();
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
  for (auto& t : ioc_threads_)
  {
    if (t.joinable())
      t.join();
  }
  main_dbg<0>.debug(str<>("boost::asio"), "shutdown complete");
}

// ----------------------------------------------------------------------------
bool GroxMainWindow::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == algo_form_->connect_button && event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->modifiers() == Qt::ShiftModifier)
    {
      //do what you need
      main_dbg<6>.debug("Shift click pressed");
      std::array<std::string, 5> strings{bitstamp_network_->account().API_user,
        bitstamp_network_->account().API_key, bitstamp_network_->account().API_secret,
        std::to_string(bitstamp_network_->account().tag_), bitstamp_network_->account().public_};

      // iterate over wallets to convert type from basic pointers
      // @TODO - improve this
      std::vector<ledger_wallet> wallets;
      auto x1 = xrpl_network::get_xrpl_instance(false)->wallets();
      auto x2 = xrpl_network::get_xrpl_instance(true)->wallets();
      ranges::for_each(x1, [&](basic_account* b) {
        ledger_wallet w = *static_cast<ledger_wallet*>(b);
        wallets.push_back(w);
      });
      ranges::for_each(x2, [&](basic_account* b) {
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
void GroxMainWindow::connect_gui_controls()
{
  // to capture ctrl-click on connect button
  algo_form_->connect_button->installEventFilter(this);

  // action for quit (@TODO)
  // connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));

  // button-click : fetch latest account balance data
  // connect(algo_form_->account_update, SIGNAL(clicked()), this, SLOT(update_account_balances()));

  // ---------------------------------------------------------------------
  // signals emitted from networking thread completion handlers should use
  // Qt::QueuedConnection to ensure they transfer to Qt main thread
  // ---------------------------------------------------------------------

  // orderbook updates from bitstamp network connection
  // 1 Priority, arbitrage, 2 plot update, 3 text update
  connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()), this, SLOT(perform_arbitrage()),
    Qt::QueuedConnection);
  //    connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()),
  //            obp_, SLOT(update_time_and_replot()), Qt::QueuedConnection);
  //    connect(bitstamp_network_.get(), SIGNAL(orderbook_changed()),
  //            this, SLOT(orderbook_text_update()), Qt::QueuedConnection);

  // when a transaction takes place we might need to update wallet/records
  connect(
    bitstamp_network_.get(), &bitstamp_network::transaction_event, this,
    [this]() {
      transaction_event();
      display_offers();
    },
    Qt::QueuedConnection);

  connect(
    bitstamp_network_.get(), &bitstamp_network::update_wallet_widget, this,
    [this](bitstamp_account* acct) {
      acct->widget_->set_data(*acct);
      display_offers();
    },
    Qt::QueuedConnection);

  connect(
    bitstamp_network_.get(), &bitstamp_network::network_initialized, this,
    [this](exchange* ex) { build_connection_gui(ex); }, Qt::QueuedConnection);

  connect(xrpl_network_.get(), SIGNAL(update_currency_widget(currency*)), this,
    SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
  connect(xrpl_testnet_.get(), SIGNAL(update_currency_widget(currency*)), this,
    SLOT(update_currency_widget(currency*)), Qt::QueuedConnection);
  connect(xrpl_network_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)), this,
    SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
  connect(xrpl_testnet_.get(), SIGNAL(update_wallet_widget(ledger_wallet*)), this,
    SLOT(update_wallet_widget(ledger_wallet*)), Qt::QueuedConnection);
  //    connect(xrpl_network_.get(), SIGNAL(orderbook_changed()),
  //            obp_, SLOT(update_time_and_replot()), Qt::QueuedConnection);
  //    connect(xrpl_network_.get(), SIGNAL(orderbook_changed()),
  //            this, SLOT(orderbook_text_update()), Qt::QueuedConnection);

  // when a transaction takes place we might need to update wallet/records
  connect(xrpl_network_.get(), SIGNAL(transaction_event()), this, SLOT(transaction_event()),
    Qt::QueuedConnection);

  connect(
    xrpl_network_.get(), &xrpl_network::network_initialized, this,
    [this](exchange* ex) { build_connection_gui(ex); }, Qt::QueuedConnection);

  connect(
    xrpl_testnet_.get(), &xrpl_network::network_initialized, this,
    [this](exchange* ex) { build_connection_gui(ex); }, Qt::QueuedConnection);

  connect(
    algo_form_->exec_algo, &QAbstractButton::clicked, this,
    [this]() {
      // execute_filter();
    },
    Qt::QueuedConnection);

  connect(
    algo_form_->run_filter, &QPushButton::clicked, this,
    [this]() {
      // pplot_dbg<0>.error(str<>("emit execute_filter"));
      // execute_filter();
    },
    Qt::QueuedConnection);

  qs_shutdown_ = new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::Key_Q)), this, SLOT(close()));
  qs_darkmode_ = new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::Key_D)), this, [this]() {
    dark_mode_ = (dark_mode_ + 1) % 3;
    LoadStyleSheet(dark_mode_);
  });
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::update_currency_widget(currency* c)
{
  assert(c->widget_);
  c->widget_->set_data(c);
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void GroxMainWindow::update_wallet_widget(ledger_wallet* w)
{
  assert(w->widget_);
  w->widget_->set_data(*w);
  display_offers();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_xrp()
{
  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(
    this, "Confirm", "Execute transaction?", QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes)
  {
    main_dbg<0>.debug(str<>("Yes clicked"));
    //        std::uint32_t tag = bitstamp_network_->account().tag_;
    //        bool test = make_xrp_payment(ripple::KeyType::secp256k1,
    //                global_settings.xrpl_wallets[    global_settings.active_wallet].private_,
    //                global_settings.xrpl_wallets[    global_settings.active_wallet].public_,
    //            bitstamp_network_->account().public_, tag, 10);

    QApplication::quit();
  }
  else
  {
    main_dbg<0>.debug(str<>("Yes *not* clicked"));
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::execute_usd()
{
  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(
    this, "Confirm", "Execute transaction?", QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes)
  {
    main_dbg<0>.debug(str<>("Yes clicked"));
    QApplication::quit();
  }
  else
  {
    main_dbg<0>.debug(str<>("Yes *not* clicked"));
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::capture_image()
{
  return;
  //    auto image = algo_form_->tabWidget->grab();
  //    algo_form_->imagelabel->setPixmap(image);
  //    algo_form_->imagelabel->setScaledContents(true);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::start_io_threads(int nthreads)
{
  static bool initialized = false;
  if (!initialized)
  {
    ioc_threads_.reserve(nthreads);
    // Run the I/O service on some threads.
    for (int i = 0; i < nthreads; ++i)
    {
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
void GroxMainWindow::perform_arbitrage()
{
  double budget = 100000;
  std::string arbitrage_string;
  double test_offset = 0.00;
  if (algo_form_->arbitrage_test_mode->isChecked())
  {
    try
    {
      test_offset = std::stod(algo_form_->arbitrage_test_offset->text().toStdString());
    }
    catch (...)
    {
      test_offset = 0.00;
    }
  }

  //
  fee_data sell_fee{0.12, 0.0};
  fee_data buy_fee{0.0, 0.01};
  //
  if (algo_form_->enable_arbitrage->isChecked())
  {
    // @TODO fix arbitrage for CP
    //    xrpl_network_->get_orderbook().compute_arbitrage(
    //      bitstamp_network_->get_orderbook(), budget, buy_fee, sell_fee, test_offset, arbitrage_string);

    if (arbitrage_string.size() > 0)
    {
      QString arb_string = QString::fromStdString(arbitrage_string);
      algo_form_->arbitrage_orders->setPlainText(arb_string);
    }
    else
    {
      algo_form_->arbitrage_orders->setPlainText("");
    }
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::transaction_event()
{
  main_dbg<5>.debug("transaction_event : update balances?");
}

// ----------------------------------------------------------------------------
void GroxMainWindow::orderbook_text_update()
{
  //  QString datastring = QString::fromStdString(xrpl_network_->get_orderbook().order_text);
  //  algo_form_->order_book_xrpl->setPlainText(datastring);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::display_offers()
{
  // Delete previous space in offer window
  for (int i = 0; i < orders_frame_->layout()->count(); ++i)
  {
    QLayoutItem* layoutItem = orders_frame_->layout()->itemAt(i);
    if (layoutItem->spacerItem())
    {
      orders_frame_->layout()->removeItem(layoutItem);
      delete layoutItem;
      --i;
    }
  }

  // for each wallet on each network
  for (auto network : global_settings.networks_)
  {
    for (auto w : network->wallets())
    {
      check_trades_dialog::create_trade_widgets(orders_frame_, w->name_, w->offers_);
    }
  }
  // absorb any extra space in the parent by adding a spacer
  orders_frame_->layout()->addItem(
    new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

// ----------------------------------------------------------------------------
// Load/Save of ini configuration/settings for main Qt application
// ----------------------------------------------------------------------------
void GroxMainWindow::closeEvent(QCloseEvent* event)
{
  saveWindowSettings();
  saveTrustlines();
  saveConnectionSetups();
  QMainWindow::closeEvent(event);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  // load settings on startup window display only
  static bool only_once = true;
  if (only_once)
  {
    loadWindowSettings();
    only_once = false;
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveTrustlines()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // Start Grox MainWindow section
  settings.beginGroup("Trustlines");
  for (auto const& t : currency::trustlines)
  {
    std::string key = hex_to_currency(t.code_);
    settings.setValue(key.c_str(), t.issuer_.c_str());
  }
  settings.endGroup();
  main_dbg<0>.debug(str<>("Trustlines saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadTrustlines()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // Start "Trustlines" section
  settings.beginGroup("Trustlines");
  QStringList childKeys = settings.childKeys();
  for (auto const& k : childKeys)
  {
    std::string code = k.toStdString();
    std::string issuer = settings.value(k).toString().toStdString();
    currency::trustlines.push_back({issuer, currency_to_hex(code)});
  }
  settings.endGroup();
  main_dbg<0>.debug(str<>("Trustlines loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveConnectionSetups()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  // ------------------------------------
  // Start "Tickers" section and remove all existing values
  settings.beginGroup("Tickers");
  settings.remove("");
  for (auto const& e : exchange_list_)
  {
    for (auto const& t : e->tickers_subscribed())
    {
      std::string key = std::string(e->name()) + "/" + currency_pair_string(t.first);
      settings.setValue(key.c_str(), true);
    }
  }
  settings.endGroup();

  // ------------------------------------
  // Start "Streams" section and remove all existing values
  settings.beginGroup("Streams");
  settings.remove("");
  for (auto const& e : exchange_list_)
  {
    // begin exchange group
    settings.beginGroup(QString(e->name().data()));

    // streams supported by this exchange
    auto streams = e->websocket_streams();

    // for each ticker we are subscribed to
    for (auto const& t : e->tickers_subscribed())
    {
      // begin ticker group
      std::string key = currency_pair_string(t.first);
      settings.beginGroup(QString(key.data()));

      // for each stream available
      for (auto const& s : streams)
      {
        std::string key = stream_to_text(s);
        main_dbg<6>.debug(str<>("Stream subscribed?"), settings.group().toStdString(), key);
        bool subscribed = e->stream_subscribed(currency_pair_string(t.first) + "/" + key);
        settings.setValue(key.c_str(), subscribed);
        if (subscribed)
          main_dbg<0>.debug(str<>("Stream subscribed"), settings.group().toStdString(), key);
      }
      settings.endGroup();    // ticker
    }
    settings.endGroup();    // exchange
  }
  settings.endGroup();    // streams
  main_dbg<0>.debug(str<>("Connections saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadConnectionSetups()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  // ------------------------------------
  settings.beginGroup("Tickers");
  for (auto const& e : exchange_list_)
  {
    settings.beginGroup(QString(e->name().data()));
    QStringList childKeys = settings.childKeys();
    for (auto const& k : childKeys)
    {
      std::string currencypair = k.toStdString();
      bool enabled = settings.value(k).toBool();
      if (enabled)
      {
        main_dbg<0>.debug(str<>("Enable Ticker"), currencypair);
        auto cp = string_to_pair(currencypair, "-");
        e->ticker_subscribe(std::get<0>(cp), std::get<1>(cp));
      }
    }
    settings.endGroup();
  }
  settings.endGroup();

  // ------------------------------------
  settings.beginGroup("Streams");
  for (auto const& e : exchange_list_)
  {
    // begin exchange group
    settings.beginGroup(QString(e->name().data()));

    // streams supported by this exchange
    auto streams = e->websocket_streams();

    // for each ticker we are subscribed to
    for (auto const& t : e->tickers_subscribed())
    {
      // begin ticker group
      std::string key = currency_pair_string(t.first);
      settings.beginGroup(QString(key.data()));

      // for each available stream
      for (auto const& s : streams)
      {
        // begin stream group
        std::string key = stream_to_text(s);
        bool subscribed = settings.value(key.c_str()).toBool();

        main_dbg<6>.debug(str<>("Stream check"), settings.group().toStdString(), key);
        if (subscribed)
        {
          main_dbg<0>.debug(str<>("Stream"), "subscribing", settings.group().toStdString(), key);
          e->stream_subscribe(io_contexts_, t.first, s, true);
        }
      }
      settings.endGroup();    // ticker
    }
    settings.endGroup();    // exchange
  }
  settings.endGroup();    // streams
  main_dbg<0>.debug(str<>("Connections loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveWindowSettings()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  settings.beginGroup("StyleSheet");
  settings.setValue("Dark", dark_mode_);
  settings.endGroup();

  // Mainwindow
  settings.beginGroup("MainWindow");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  settings.endGroup();

  // Dockwindow perspectives
  settings.beginGroup("DockWindow_Perspectives");
  global_settings.dock_manager_->savePerspectives(settings);
  settings.setValue("active", active_perspective_);
  settings.endGroup();

  main_dbg<0>.debug(str<>("Settings saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadWindowSettings()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  settings.beginGroup("StyleSheet");
  dark_mode_ = settings.value("Dark").toInt();
  LoadStyleSheet(dark_mode_);
  settings.endGroup();

  // MainWindow section
  settings.beginGroup("MainWindow");
  restoreGeometry(settings.value("geometry").toByteArray());
  restoreState(settings.value("windowState").toByteArray());
  settings.endGroup();

  // Dockwindow perspectives
  settings.beginGroup("DockWindow_Perspectives");
  global_settings.dock_manager_->loadPerspectives(settings);
  createPerspectives_Ui();
  if (settings.contains("active"))
  {
    openPerspective(settings.value("active", "Default").toString());
  }
  settings.endGroup();

  main_dbg<0>.debug(str<>("Settings loaded"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::stream_process(ohlctv_sample const& ohlc)
{
  main_dbg<0>.debug(str<>("New data"), msecs_unix_to_calendar_time(ohlc.time), ohlc);
  //    df_.process(ohlc);
}

// ----------------------------------------------------------------------------
QColor colours[10] = {QColor("cyan"), QColor("magenta"), QColor("red"), QColor("darkRed"),
  QColor("darkCyan"), QColor("darkMagenta"), QColor("green"), QColor("darkGreen"), QColor("yellow"),
  QColor("blue")};
/*
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
        price_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);

        filters_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
        filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

        assets_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
        assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);
        return;
    }

    bool all_resolutions = false;
    int algorithm = ta_dialog.algorithm();

    std::vector<candle_res> resolutions = ohlc_data_resolutions::available_resolutions();
    candle_res base_resolution = ohlc_data_resolutions::minute;
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

//    base_resolution = ohlc_data_resolutions::minute;

    auto dataset =     global_settings.data_manager_->get_dataset(base_resolution);
    auto data = dataset->ohlc_samples_;

    // NB. Inputs need to be used by reference, with the original object
    // kept alive so that they can be updated continuously.
    // Other filter objects are copied during pipeline construction

    // initialize an ohlc input object with the first dataset value
    input_value"data/ohlctv_sample.hpp" ohlc_in(data->data().front());
    pipeline::input<ohlctv_sample const&> ohlc_input = std::ref(ohlc_in);

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
    //start = algo_form_->repair_date->dateTime();
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
        time_in.set(ohlc.time + base_resolution.res_); + ohlc_data_resolutions::minute);

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
                double p =     global_settings.data_manager_->get_estimated_buy_price(funding[0].usd, e.time_, 2.0);
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
                double p =     global_settings.data_manager_->get_estimated_sell_price(funding[0].xrp, e.time_, 2.0);
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
        price_plot_->add_price_curve("MA", data, colours[col++]);
    }
    price_plot_->add_buy_sell_curve("Buy",  buys,  Qt::green);
    price_plot_->add_buy_sell_curve("Sell", sells, Qt::red);
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
*/

// ----------------------------------------------------------------------------
void GroxMainWindow::build_connection_gui(exchange* ex)
{
  // widget with panels for tickers/selected/streams
  connection_widget* conwidget = new connection_widget(this, io_contexts_, ex);
  conwidget->setup_gui();
  net_layout_->insertWidget(0, conwidget);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::createPerspectives_Ui()
{
  // create one time setup menu items
  if (!global_settings.dockwindows_menu_)
  {
    // main window menu entry
    QMenu* docking_menu_ = new QMenu("Window");
    ui.menubar->addMenu(docking_menu_);
    // subsection for dockwindows
    QAction* menuentry_ = docking_menu_->addAction("Dock windows...");
    global_settings.dockwindows_menu_ = new QMenu();
    menuentry_->setMenu(global_settings.dockwindows_menu_);
    // subsection for perspectives
    QAction* menuentry2_ = docking_menu_->addAction("Perspectives...");
    perspectives_menu_ = new QMenu();
    menuentry2_->setMenu(perspectives_menu_);
    // action to create a new perspective
    QAction* SavePerspectiveAction = new QAction("Save Perspective");
    connect(SavePerspectiveAction, SIGNAL(triggered()), this, SLOT(savePerspective()));
    docking_menu_->addAction(SavePerspectiveAction);
  }
  //
  perspectives_menu_->clear();
  for (QString const& name : global_settings.dock_manager_->perspectiveNames())
  {
    QAction* LoadPerspectiveAction = new QAction(name);
    LoadPerspectiveAction->setCheckable(true);
    connect(
      LoadPerspectiveAction, &QAction::triggered, this, [this, name]() { openPerspective(name); },
      Qt::QueuedConnection);
    perspectives_menu_->addAction(LoadPerspectiveAction);
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::savePerspective()
{
  QString Name = QInputDialog::getText(
    this, "Save Perspective", "Enter name:", QLineEdit::Normal, active_perspective_);
  if (!Name.isEmpty())
  {
    global_settings.dock_manager_->addPerspective(Name);
    createPerspectives_Ui();
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::openPerspective(QString const& name)
{
  active_perspective_ = name;
  for (auto* action : perspectives_menu_->actions())
  {
    if (action->text() == name)
    {
      action->setChecked(true);
    }
    else
    {
      action->setChecked(false);
    }
  }
  global_settings.dock_manager_->openPerspective(name);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::LoadStyleSheet(int dark)
{
#ifdef GROX_DEBUG_RESOURCES
  QDirIterator it(":", QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    qDebug() << it.next();
  }
#endif
  QString name;
  if (dark == 0)
  {
    global_settings.dock_manager_->setStyleSheet("");
    qApp->setStyleSheet("");
    return;
  }
  else if (dark == 1)
  {
    name = ":qdarkstyle/dark/darkstyle.qss";
  }
  else if (dark == 2)
  {
    name = ":qdarkstyle/light/lightstyle.qss";
  }
  QFile f(name);
  if (!f.exists())
  {
    main_dbg<0>.error(str<>("Stylesheet"), "Unable to set stylesheet, file not found");
  }
  else
  {
    f.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&f);
    global_settings.dock_manager_->setStyleSheet("");
    qApp->setStyleSheet(ts.readAll());
  }
}
