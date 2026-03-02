// STL
#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// Qt
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QFrame>
#include <QInputDialog>
#include <QKeyCombination>
#include <QKeySequence>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QShortcut>
// Qwt
#include <QwtAxis>
#include <QwtScaleDraw>
#include <QwtScaleEngine>
// Grox
#include "currency/json_data_types.hpp"
#include "currency/ohlctv_sample.hpp"
#include "debug/demangle_helper.hpp"
#include "debug/logging.hpp"
#include "exchange/xrpl.hpp"
#include "exchange/xrpl_network.hpp"
#include "io/hdf5_ohlc_manager.hpp"
#include "mainwindow.hpp"
#include "network/evp-encrypt.hpp"
#include "senders/qtstdexec.hpp"
#include "util/datetime_utils.hpp"
#include "util/stringutils.hpp"
#include "widgets/check_trades_dialog.hpp"
#include "widgets/connection_widget.hpp"
#include "widgets/currency_widget.hpp"
#include "widgets/password_dialog.hpp"
#include "widgets/price_chart_widget.hpp"
#include "widgets/trade_algorithm_widget.hpp"
#include "widgets/wallet_widget.hpp"
#include "widgets_control/indicator_json.hpp"
#include "widgets_control/indicator_widget.hpp"

// Qt Advanced Docking System
#include "AutoHideDockContainer.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"

// qtermwidget
#include "DockManager.h"
#include "FloatingDockContainer.h"
#include "qtermwidget.h"

#define GROX_HAVE_BITSTAMP
#define GROX_HAVE_XRPL

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

// ----------------------------------------------------------------------------
static auto main_log = grox::log::create("Main-win");

using namespace ads;

namespace {
  QStringList terminal_color_scheme_dirs;

  void ensure_terminal_color_schemes()
  {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    terminal_color_scheme_dirs.clear();

    QString const app_dir = QCoreApplication::applicationDirPath();
    QStringList const candidates = {QDir::cleanPath(app_dir + "/color-schemes"),
        QDir::cleanPath(app_dir + "/../color-schemes"),
        QDir::cleanPath(app_dir + "/../share/qtermwidget6/color-schemes"),
        QDir::cleanPath(app_dir + "/../../share/qtermwidget6/color-schemes"),
        QDir::cleanPath(app_dir + "/../../../share/qtermwidget6/color-schemes"),
        QDir::cleanPath(QStringLiteral(GROX_SOURCE_DIR) + "/extern/qtermwidget/lib/color-schemes")};

    for (QString const& dir : candidates)
    {
      if (QDir(dir).exists())
      {
        QTermWidget::addCustomColorSchemeDir(dir);
        terminal_color_scheme_dirs.append(dir);
      }
    }
  }

  void apply_terminal_theme(QTermWidget* terminal, int dark_mode)
  {
    if (!terminal) return;
    ensure_terminal_color_schemes();
    QString const desired_dark = "WhiteOnBlack";
    QString const desired_light = "Linux";
    QStringList const schemes = QTermWidget::availableColorSchemes();
    QString chosen = (dark_mode == 1) ? desired_dark : desired_light;
    if (!schemes.contains(chosen))
    {
      if (schemes.contains(desired_dark)) { chosen = desired_dark; }
      else if (schemes.contains(desired_light)) { chosen = desired_light; }
    }
    terminal->setColorScheme(chosen);
#ifdef QT_DEBUG
    static bool logged_dirs_once = false;
    if (!logged_dirs_once)
    {
      qDebug() << "qtermwidget color-scheme dirs:" << terminal_color_scheme_dirs;
      qDebug() << "qtermwidget available schemes:" << schemes;
      logged_dirs_once = true;
    }
    qDebug() << "qtermwidget applied scheme:" << chosen << "for dark_mode=" << dark_mode;
#endif
  }

  QTermWidget* create_terminal_widget(int dark_mode)
  {
    auto* terminal = new QTermWidget(1);    // 1 = start shell immediately
    terminal->setTerminalFont(QFont("Monospace", 10));
    terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    apply_terminal_theme(terminal, dark_mode);
    return terminal;
  }
}    // namespace

// ----------------------------------------------------------------------------
std::shared_ptr<price_chart_widget> create_price_chart_widget(
    std::shared_ptr<ohlc_dataset_view> view, ticker::data tdata, currency_pair cp)
{
  // create a new price plot object, we don't use std::make_shared because
  // destruction of widget might use pointer instead of shared_ptr
  auto* pcw = new price_chart_widget(nullptr, view, tdata->exchange_, cp);
  std::shared_ptr<price_chart_widget> chart_widget(pcw);

  // put the price plot into a dock widget
  using namespace ads;
  std::string title = currency_pair_string(cp) + " price " + tdata->exchange_->get_name();
  CDockWidget* PlotDockWidget = new CDockWidget(global_settings.dock_manager_, to_qstring(title));
  PlotDockWidget->setWidget(chart_widget.get(), CDockWidget::AutoScrollArea);
  PlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  global_settings.dock_manager_->addDockWidgetFloating(PlotDockWidget);
  global_settings.dockwindows_menu_->addAction(PlotDockWidget->toggleViewAction());
  return chart_widget;
}

// ----------------------------------------------------------------------------
QPlainTextEdit* create_order_book_text_widget(std::string cps, std::string name)
{
  // create a new orderbook text display
  size_t const font_size = 8;
  auto* orderbook_text = new QPlainTextEdit(nullptr);
  QString txt = "X";
  int char_size = QFontMetrics(orderbook_text->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  orderbook_text->setMinimumWidth(calcWidth);
  QFont font = QFont();
  font.setPointSize(font_size);
  font.setFamily("Courier");
  orderbook_text->setFont(font);

  // put the order book into a dock widget
  using namespace ads;
  std::string obtitle = cps + " text " + name;
  CDockWidget* obPlotDockWidget =
      new CDockWidget(global_settings.dock_manager_, to_qstring(obtitle));
  obPlotDockWidget->setWidget(orderbook_text, CDockWidget::AutoScrollArea);
  obPlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  global_settings.dock_manager_->addDockWidgetFloating(obPlotDockWidget);
  global_settings.dockwindows_menu_->addAction(obPlotDockWidget->toggleViewAction());

  return orderbook_text;
}

// ----------------------------------------------------------------------------
OrderBookPlot* create_order_book_plot_widget(
    std::string cps, std::string name, std::shared_ptr<order_book_base> orderbook)
{
  using namespace ads;

  OrderBookPlot* orderbook_plot = new OrderBookPlot(nullptr, orderbook);
  orderbook_plot->setMinimumSize(384, 256);
  //
  std::string obptitle = cps + " depth " + name;
  CDockWidget* obpDockWidget = new CDockWidget(global_settings.dock_manager_, to_qstring(obptitle));
  obpDockWidget->setWidget(orderbook_plot, CDockWidget::AutoScrollArea);
  obpDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  global_settings.dock_manager_->addDockWidgetFloating(obpDockWidget);
  global_settings.dockwindows_menu_->addAction(obpDockWidget->toggleViewAction());
  return orderbook_plot;
}

// ----------------------------------------------------------------------------
void create_ticker_price_plot(ticker::data tdata, currency_pair cp)
{
  tdata->chart_widget_ = create_price_chart_widget(tdata->view_, tdata, cp);
  // start by displaying 1 day of data
  tdata->chart_widget_->graph_rescale(0);

  auto replot_live_data = [tdata](currency_pair cp, grox::live_trade_data t) {
    auto p = t.price;
    auto v = t.amount;
    ohlctv_sample new_sample(1000.0 * std::atof(t.timestamp.c_str()), p, p, p, p, v);
    tdata->view_->add_live_data(new_sample);
    QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
      // check pointers in case messages arrive after cleanup has started
      if (tdata && tdata->chart_widget_) tdata->chart_widget_->update_live_data(new_sample);
    });
  };
  tdata->live_trade_subscribers_.subscribe("price_plot", replot_live_data);

  auto replot_chart = [tdata](ticker::data source) {
    assert(tdata.get() == source.get());
    QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
      if (tdata->chart_widget_) tdata->chart_widget_->replot();
    });
  };
  tdata->price_data_subscribers_.subscribe("price_plot", replot_chart);
}

// ----------------------------------------------------------------------------
void create_ticker_orderbook_widgets(ticker::data tdata, currency_pair cp)
{
  std::string exch_name = std::string(tdata->exchange_->get_name());
  std::string cps = currency_pair_string(cp);
  auto* orderbook_text = create_order_book_text_widget(cps, exch_name);
  auto* orderbook_plot = create_order_book_plot_widget(cps, exch_name, tdata->orderbook_);

  auto orderbook_text_sub = [tdata, orderbook_text](currency_pair cp) {
    QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
      GROX_LOG_TRACE(main_log, "{:>20} {}", "Orderbook-Text", "orderbook_plot_sub");
      // check pointers in case messages arrive after cleanup has started
      if (tdata && tdata->orderbook_)
      {
        QString datastring = QString::fromStdString(tdata->orderbook_->get_orderbook_string());
        orderbook_text->setPlainText(datastring);
      }
    });
  };
  tdata->orderbook_subscribers_.subscribe("orderbook_text", orderbook_text_sub);

  auto orderbook_plot_sub = [tdata, orderbook_plot](currency_pair cp) {
    QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
      GROX_LOG_TRACE(main_log, "{:>20} {}", "Orderbook-Plot", "orderbook_plot_sub");
      // check pointers in case messages arrive after cleanup has started
      if (tdata && tdata->orderbook_)
      {
        orderbook_plot->update_graph_limits();
        orderbook_plot->new_data_event();
        orderbook_plot->update_time_and_replot();
      }
    });
  };
  tdata->orderbook_subscribers_.subscribe("orderbook_plot", orderbook_plot_sub);
}

// ----------------------------------------------------------------------------
void ticker_stream_gui_constructor(currency_pair cp, ticker::data tdata, ticker::streams stream)
{
  GROX_LOG_TRACE(main_log, "{:>20} {}", "Stream", "factory_create");
  if (stream == ticker::streams::price_data)
    create_ticker_price_plot(tdata, cp);
  else if (stream == ticker::streams::order_book)
    create_ticker_orderbook_widgets(tdata, cp);
  else
    GROX_LOG_ERROR(main_log, "{:>20} {} {}", "Stream", "factory_create no GUI for stream",
        ticker::stream_names[stream]);
}

// ----------------------------------------------------------------------------
void ticker_stream_gui_destructor(currency_pair cp, ticker::data tdata, ticker::streams stream)
{
  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Stream", "factory_destroy");
  if (stream == ticker::streams::price_data)
  {
    tdata->chart_widget_->parentWidget()->deleteLater();
    tdata->chart_widget_.reset();
    tdata->live_trade_subscribers_.clear();
    tdata->view_.reset();
    GROX_LOG_ERROR(main_log, "{:>20} {}", "Stream", tdata->chart_widget_.use_count());
    GROX_LOG_ERROR(main_log, "{:>20} {}", "Stream", tdata->view_.use_count());
  }
  else if (stream == ticker::streams::order_book)
  {
    //    tdata->orderbook_->parentWidget()->deleteLater();
    tdata->orderbook_.reset();
  }
  else
    GROX_LOG_ERROR(main_log, "{:>20} {}", "Stream", "factory_destroy unknown stream");
}

// ----------------------------------------------------------------------------
GroxMainWindow::GroxMainWindow(QWidget* parent)
  : QMainWindow(parent)
  , orders_frame_(nullptr)
  , accounts_frame_(nullptr)
  , terminal_widget_(nullptr)
  , terminal_dock_widget_(nullptr)
  , net_layout_(nullptr)
  , perspectives_menu_(nullptr)
  , qs_shutdown_(nullptr)
  , qs_darkmode_(nullptr)
  , qs_password_(nullptr)
  , qs_terminal_(nullptr)
  , qs_arbitrage_(nullptr)
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
  CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);
  global_settings.dock_manager_ = new CDockManager(this);

  // // Set central widget
  // QPlainTextEdit* w = new QPlainTextEdit();
  // w->setPlaceholderText("This is the central editor. Enter your text here.");
  // CDockWidget* CentralDockWidget = new CDockWidget("CentralWidget");
  // CentralDockWidget->setWidget(w);
  // auto* CentralDockArea = global_settings.dock_manager_->setCentralWidget(CentralDockWidget);
  // CentralDockArea->setAllowedAreas(DockWidgetArea::OuterDockAreas);

  // ----------------------------------
  // Setup a menu to allow dockwindow control - must be after dock manager creation
  createPerspectives_Ui();

  // ----------------------------------
  // Create dockwidget for network connections
  net_layout_ = new QTabWidget(this);
  net_layout_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  //
  CDockWidget* NetworkDockWidget = new CDockWidget(global_settings.dock_manager_, "Networks");
  NetworkDockWidget->setWidget(net_layout_, CDockWidget::AutoScrollArea);
  NetworkDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  NetworkDockWidget->setMinimumSize(128, 196);
  auto const NetworkautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
      SideBarLocation::SideBarRight, NetworkDockWidget);
  NetworkautoHideContainer->setSize(256);
  global_settings.dockwindows_menu_->addAction(NetworkDockWidget->toggleViewAction());

  // ----------------------------------
  // create a dock widget to hold accounts/wallets
  accounts_frame_ = new QFrame();
  accounts_frame_->setLayout(new QVBoxLayout());
  //
  CDockWidget* AccountsDockWidget = new CDockWidget(global_settings.dock_manager_, "Accounts");
  AccountsDockWidget->setWidget(accounts_frame_, CDockWidget::AutoScrollArea);
  AccountsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  AccountsDockWidget->setMinimumSize(128, 196);
  auto const AccountsautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
      SideBarLocation::SideBarRight, AccountsDockWidget);
  AccountsautoHideContainer->setSize(256);
  global_settings.dockwindows_menu_->addAction(AccountsDockWidget->toggleViewAction());

  // ----------------------------------
  // create a dock widget to hold open orders
  orders_frame_ = new QFrame();
  orders_frame_->setLayout(new QVBoxLayout());
  //
  CDockWidget* OrdersDockWidget = new CDockWidget(global_settings.dock_manager_, "Trades");
  OrdersDockWidget->setWidget(orders_frame_, CDockWidget::AutoScrollArea);
  OrdersDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContentMinimumSize);
  OrdersDockWidget->setMinimumSize(128, 196);
  auto const OrdersautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
      SideBarLocation::SideBarRight, OrdersDockWidget);
  OrdersautoHideContainer->setSize(256);
  global_settings.dockwindows_menu_->addAction(OrdersDockWidget->toggleViewAction());

  // ----------------------------------
  // create a dock widget for terminal
  terminal_widget_ = create_terminal_widget(dark_mode_);
  terminal_dock_widget_ = new CDockWidget(global_settings.dock_manager_, "Terminal");
  terminal_dock_widget_->setWidget(terminal_widget_);
  terminal_dock_widget_->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  terminal_dock_widget_->setMinimumSize(400, 300);
  global_settings.dock_manager_->addDockWidget(
      DockWidgetArea::BottomDockWidgetArea, terminal_dock_widget_);
  global_settings.dockwindows_menu_->addAction(terminal_dock_widget_->toggleViewAction());

#ifdef GROX_HAVE_BITSTAMP
  // ----------------------------------
  // create bitstamp abstract_exchange interface
  bitstamp_network_ = bitstamp_network::get_bitstamp_instance();
  exchange_list_.push_back(bitstamp_network_);

  // @TODO - this should be moved into accounts frame
  // when a transaction takes place we might need to update wallet/records
  connect(
      bitstamp_network_.get(), &bitstamp_network::transaction_event, this,
      [this]() {
        transaction_event();
        display_offers();
      },
      Qt::QueuedConnection);

  connect(bitstamp_network_.get(), SIGNAL(wallet_changed(ledger_wallet*)), this,
      SLOT(wallet_changed(ledger_wallet*)), Qt::QueuedConnection);

  connect(
      bitstamp_network_.get(), &bitstamp_network::network_initialized, this,
      [this](abstract_exchange* ex) {
        ex->register_factory("ticker_subscribe",
            [ex](currency_pair cp, ticker::data, ticker::streams) { ex->ticker_subscribe(cp); });
        ex->register_factory("stream_subscribe", ticker_stream_gui_constructor);
        ex->register_factory("stream_unsubscribe", ticker_stream_gui_destructor);
        // widget with panels for tickers/selected/streams
        connection_widget* conwidget = new connection_widget(this, ex);
        conwidget->setup_gui();
        net_layout_->addTab(conwidget, to_qstring(ex->get_name()));
        global_settings.dock_manager_->openPerspective(active_perspective_);
      },
      Qt::QueuedConnection);
#endif

#ifdef GROX_HAVE_XRPL
  // ----------------------------------
  // create xrp network interfaces
  xrpl_network_ = xrpl_network::get_xrpl_instance(false);
  xrpl_testnet_ = xrpl_network::get_xrpl_instance(true);
  exchange_list_.push_back(xrpl_network_);
  exchange_list_.push_back(xrpl_testnet_);

  // wallet update
  connect(xrpl_network_.get(), SIGNAL(wallet_changed(ledger_wallet*)), this,
      SLOT(wallet_changed(ledger_wallet*)), Qt::QueuedConnection);
  connect(xrpl_testnet_.get(), SIGNAL(wallet_changed(ledger_wallet*)), this,
      SLOT(wallet_changed(ledger_wallet*)), Qt::QueuedConnection);

  // when a transaction takes place we might need to update wallet/records
  connect(xrpl_network_.get(), SIGNAL(transaction_event()), this, SLOT(transaction_event()),
      Qt::QueuedConnection);

  connect(
      xrpl_network_.get(), &xrpl_network::network_initialized, this,
      [this](abstract_exchange* ex) {
        ex->register_factory("ticker_subscribe",
            [ex](currency_pair cp, ticker::data, ticker::streams) { ex->ticker_subscribe(cp); });
        ex->register_factory("stream_subscribe", ticker_stream_gui_constructor);
        ex->register_factory("stream_unsubscribe", ticker_stream_gui_destructor);
        // widget with panels for tickers/selected/streams
        connection_widget* conwidget = new connection_widget(this, ex);
        conwidget->setup_gui();
        net_layout_->addTab(conwidget, to_qstring(ex->get_name()));
        global_settings.dock_manager_->openPerspective(active_perspective_);
      },
      Qt::QueuedConnection);

  connect(
      xrpl_testnet_.get(), &xrpl_network::network_initialized, this,
      [this](abstract_exchange* ex) {    // widget with panels for tickers/selected/streams
        connection_widget* conwidget = new connection_widget(this, ex);
        conwidget->setup_gui();
        net_layout_->addTab(conwidget, to_qstring(ex->get_name()));
        global_settings.dock_manager_->openPerspective(active_perspective_);
      },
      Qt::QueuedConnection);
#endif

  // @TODO move this into account frame
  // for each wallet on each network
  for (auto network : global_settings.networks_)
  {
    for (auto w : network->wallets())
    {
      // create a gui widget for the wallet
      auto* widget = new wallet_widget(this);
      std::string wname = fmt::format("{}/{}", w->network_->get_name(), w->name_);
      widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
      widget->setObjectName(to_qstring(wname));
      GROX_LOG_DEBUG(main_log, "{:>20} {} {}", "wallet_widget", "set_data", wname);
      if (startswith(network->get_name(), "Bitstamp"))
        widget->set_data(static_cast<bitstamp_account*>(w));
      if (startswith(network->get_name(), "XRPL")) widget->set_data(static_cast<ledger_wallet*>(w));
      accounts_frame_->layout()->addWidget(widget);

      // ------------------------------------------------------------
      // create a callback that constructs a currency exchange widget
      auto callback1 = [this, network]() {
        auto alg = indicators::indicator_registry::find_by_name("Currency-Exchange");
        nlohmann::json defaults = {//
            {"Order-Book-1",       // default values for widget controls
                {
                    {"label", "Currency pairs"},
                    {"exchanges", std::vector<std::string>{network->get_name()}},
                    {"tickers-0_value", currency_pair_string({{"XRP"}, {"USD"}}, "-", false)},
                    {"tickers-1_value", currency_pair_string({{"XRP"}, {"GBP"}}, "-", false)},
                }}};

        indicator_widget widget(alg, defaults);
        auto result = widget.execute_as_dialog();
        if (result == QDialog::Accepted)
        {
          auto alg_copy = alg->create(alg.get());
          auto algowidget_ = trade_widget_factory(alg_copy, exchange_list_);
          trade_widgets_.push_back(dock_trading_widget(algowidget_, alg_copy->get_name()));
        }
      };

      // ------------------------------------------------------------
      // create a callback that constructs an arbitrage widget
      auto callback2 = [this, network]() {
        auto alg = indicators::indicator_registry::find_by_name("Arbitrage 2-way");
        // QDialog* widget = create_trading_widget({network}, {alg});
        // trade_widgets_.push_back(widget);

        auto alg_copy = alg->create(alg.get());
        auto algowidget_ = trade_widget_factory(alg_copy, exchange_list_);
        trade_widgets_.push_back(dock_trading_widget(algowidget_, alg_copy->get_name()));
      };

      // ------------------------------------------------------------
      // create a callback that constructs a market-maker widget
      auto callback3 = [this, network]() {
        auto alg = indicators::indicator_registry::find_by_name("Market-Maker");
        // QDialog* widget = create_trading_widget({network}, {alg});
        // trade_widgets_.push_back(widget);

        auto alg_copy = alg->create(alg.get());
        auto algowidget_ = trade_widget_factory(alg_copy, exchange_list_);
        trade_widgets_.push_back(dock_trading_widget(algowidget_, alg_copy->get_name()));
      };

      for (auto algo : network->supported_trade_actions())
      {
        switch (algo)
        {
        case supported_trade_actions::currency_exchange:
          widget->add_algorithm("Currency Exchange", callback1);
          break;
        case supported_trade_actions::arbitrage_2way:
          widget->add_algorithm("Arbitrage(2)", callback2);
          break;
        case supported_trade_actions::market_maker:
          widget->add_algorithm("Market Maker", callback3);
          break;
        }
      }
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
  // initialize networks / start websocket connections etc
  for (auto const& e : exchange_list_)
  {
    GROX_LOG_DEBUG(main_log, "{:>20} {}", "Init abstract_exchange", e->get_name());
    e->initialize();
  }

  register_control_factories();
}

// ----------------------------------------------------------------------------
GroxMainWindow::~GroxMainWindow()
{
  trade_widgets_.clear();
  //
  delete qs_shutdown_;
  delete qs_darkmode_;
  delete qs_password_;
  delete qs_arbitrage_;
  delete qs_terminal_;
  // dockmanager is deleted by gui destruction
  global_settings.dock_manager_ = nullptr;
  // release all networks
  for (auto& n : global_settings.networks_) n.reset();
  // release datamanager
  global_settings.data_manager_.reset();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::appExitCleanupHandler()
{
  GROX_LOG_DEBUG(main_log, "{:>20}", "appExitCleanupHandler");
}

// ----------------------------------------------------------------------------
bool GroxMainWindow::eventFilter(QObject* obj, QEvent* event)
{
  return QWidget::eventFilter(obj, event);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::connect_gui_controls()
{
  // button-click : fetch latest account balance data
  // connect(algo_form_->account_update, SIGNAL(clicked()), this, SLOT(update_account_balances()));

  qs_shutdown_ = new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::Key_Q)), this, SLOT(close()));
  qs_darkmode_ =
      new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::SHIFT) + int(Qt::Key_D)), this, [this]() {
        dark_mode_ = (dark_mode_ + 1) % 3;
        LoadStyleSheet(dark_mode_);
      });

  qs_password_ =
      new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::SHIFT) + int(Qt::Key_P)), this, [this]() {
        GROX_LOG_DEBUG(main_log, "Shift click pressed");
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
        password_dialog npw = password_dialog(bitstamp_network_->accounts(), wallets);
        if (npw.exec() == QDialog::Accepted) { generate_encrypted_ini_data(npw); }
      });

  qs_arbitrage_ = new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::Key_M)), this, [this]() {
    QDialog* widget = create_trading_widget(exchange_list_);
    trade_widgets_.push_back(widget);
  });

  qs_terminal_ =
      new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::SHIFT) + int(Qt::Key_T)), this, [this]() {
        if (!terminal_dock_widget_) return;

        bool terminal_needs_recreate = (terminal_widget_ == nullptr);
        if (!terminal_needs_recreate)
        {
          int const shell_pid = terminal_widget_->getShellPID();
          terminal_needs_recreate = shell_pid <= 0;
        }

        if (terminal_needs_recreate)
        {
          if (terminal_widget_)
          {
            terminal_widget_->deleteLater();
            terminal_widget_ = nullptr;
          }
          terminal_widget_ = create_terminal_widget(dark_mode_);
          terminal_dock_widget_->setWidget(terminal_widget_);
        }

        auto* toggle = terminal_dock_widget_->toggleViewAction();
        if (toggle && !toggle->isChecked()) { toggle->trigger(); }
        terminal_dock_widget_->show();
        terminal_dock_widget_->raise();
      });
}

// ----------------------------------------------------------------------------
// @TODO - this should be moved into accounts frame/widget
// slot to ensure widget updates on GUI thread
void GroxMainWindow::wallet_changed(ledger_wallet* w)
{
  std::string wname = fmt::format("{}/{}", w->network_->get_name(), w->name_);
  GROX_LOG_DEBUG(main_log, "{:>20} {}", "wallet_changed", wname);
  // widgets are added to the layout, but are "owned" by the layout's parent
  auto* widget = accounts_frame_->findChild<wallet_widget*>(to_qstring(wname));
  if (widget) { widget->set_data(w); }
  else { GROX_LOG_ERROR(main_log, "{:>20} {} {}", "wallet_changed", "Failed to locate", wname); }
  display_offers();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::transaction_event()
{
  GROX_LOG_DEBUG(main_log, "transaction_event : update balances?");
}

// ----------------------------------------------------------------------------
// @TODO - this should be moved into accounts frame
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
  GROX_LOG_DEBUG(main_log, "{:>20}", "closeEvent");

  if (!exchange_list_.empty())
  {
    saveTrustlines();
    saveConnectionSetups();

    auto snd = stdexec::starts_on(QtStdExec::QThreadScheduler(), stdexec::just())    //
        | stdexec::then([this]() {
            for (auto& e : exchange_list_)
            {
              auto name = e->get_name();
              GROX_LOG_DEBUG(main_log, "{:>20} {} {}", "shut down", name, "start");
              e->shut_down();
              e.reset();
              GROX_LOG_DEBUG(main_log, "{:>20} {} {}", "shut down", name, "complete");
            }
            GROX_LOG_DEBUG(main_log, "{:>20} {}", "exchanges", "shutdown complete");
            exchange_list_.clear();
          })                                                      //
        | stdexec::continues_on(QtStdExec::QThreadScheduler())    // pika -> Qt
        | stdexec::then([this]() {
            GROX_LOG_DEBUG(main_log, "{:>20}", "Close");
            close();
          });
    stdexec::start_detached(std::move(snd));
    // we need to asynchronously shut down,
    // close all network connections, these need the application messaging loop
    // to correctly process everything (because they use Qt Networking/threads),
    // so we will ignore the close event, and call 'close' on ourselves once shutdown is ready
    GROX_LOG_DEBUG(main_log, "{:>20} {}", "closeEvent", "Ignore");
    event->ignore();
  }
  else
  {
    // just shut down since all exchanges are handled
    saveWindowSettings();
    // we make a copy of the list so that we can remove items without breaking iterators
    auto list1 = global_settings.dock_manager_->floatingWidgets();
    for (auto fdw : list1) { delete fdw; }
    auto list2 = global_settings.dock_manager_->dockContainers();
    for (auto dc : list2) { delete dc; }

    QMainWindow::closeEvent(event);
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  // load settings on startup window display only
  static bool only_once = true;
  if (only_once)
  {
    only_once = false;
    auto snd = stdexec::starts_on(QtStdExec::QThreadScheduler(), stdexec::just())    //
        | stdexec::then([this]() { loadConnectionSetups(); })                        //
        | stdexec::then([this]() { loadWindowSettings(); });                         //
    stdexec::start_detached(std::move(snd));
  }
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveTrustlines()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);
  // Start Grox MainWindow section
  settings.beginGroup("Trustlines");
  for (auto const& t : currencies::trustlines)
  {
    std::string key = hex_to_currency(t.code_);
    settings.setValue(key.c_str(), t.issuer_.c_str());
  }
  settings.endGroup();
  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Trustlines saved", settings.fileName().toStdString());
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
    currencies::trustlines.push_back({issuer, currency_to_hex(code)});
  }
  settings.endGroup();
  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Trustlines loaded", settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::saveConnectionSetups()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  // ------------------------------------
  // Start "Streams" section and remove all existing values
  settings.beginGroup("Streams");
  settings.remove("");
  for (auto const& e : exchange_list_)
  {
    // streams supported by this abstract_exchange
    auto streams = e->websocket_streams();
    // begin abstract_exchange group
    settings.beginGroup(QString::fromStdString(e->get_name()));

    // for each ticker we are subscribed to
    abstract_exchange::subscription_lock_type l;
    for (auto const& ticker : e->tickers_subscribed(l))
    {
      // begin ticker group
      auto cp = ticker.first;
      std::string key = currency_pair_string(cp);
      settings.beginGroup(to_qstring(key));

      // for each stream available
      for (auto const& s : streams)
      {
        auto skey = std::string(magic_enum::enum_name(s));
        GROX_LOG_TRACE(
            main_log, "{:>20} {} {} {}", "saveConnectionSetups", "Stream subscribed", key, skey);
        bool subscribed = e->is_stream_subscribed(cp, s);
        settings.setValue(skey.c_str(), subscribed);
        if (subscribed)
          GROX_LOG_TRACE(main_log, "{:>20} {} {} {}", "saveConnectionSetups", "Stream subscribed",
              settings.group().toStdString(), key);
      }
      settings.endGroup();    // ticker
    }
    settings.endGroup();    // abstract_exchange
  }
  settings.endGroup();    // streams
  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Connections saved", settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadConnectionSetups()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  GROX_LOG_ERROR(main_log, "{:>20} {}", "Fix connect init", settings.fileName().toStdString());
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

  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Settings saved", settings.fileName().toStdString());
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

  GROX_LOG_DEBUG(main_log, "{:>20} {}", "Settings loaded", settings.fileName().toStdString());
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
    if (action->text() == name) { action->setChecked(true); }
    else { action->setChecked(false); }
  }
  global_settings.dock_manager_->openPerspective(name);
}

// ----------------------------------------------------------------------------
void GroxMainWindow::LoadStyleSheet(int dark)
{
#ifdef GROX_DEBUG_RESOURCES
  QDirIterator it(":", QDirIterator::Subdirectories);
  while (it.hasNext()) { qDebug() << it.next(); }
#endif
  QString name;
  if (dark == 0)
  {
    global_settings.dock_manager_->setStyleSheet("");
    qApp->setStyleSheet("");
    apply_terminal_theme(terminal_widget_, dark);
    return;
  }
  else if (dark == 1) { name = ":qdarkstyle/dark/darkstyle.qss"; }
  else if (dark == 2) { name = ":qdarkstyle/light/lightstyle.qss"; }
  QFile f(name);
  if (!f.exists())
  {
    GROX_LOG_ERROR(main_log, "{:>20} {}", "Stylesheet", "Unable to set stylesheet, file not found");
  }
  else
  {
    if (!f.open(QFile::ReadOnly | QFile::Text))
    {
      GROX_LOG_ERROR(main_log, "{:>20} {}", "Stylesheet", "Unable to open stylesheet file");
    }
    else
    {
      QTextStream ts(&f);
      global_settings.dock_manager_->setStyleSheet("");
      qApp->setStyleSheet(ts.readAll());
    }
  }
  apply_terminal_theme(terminal_widget_, dark);
}
