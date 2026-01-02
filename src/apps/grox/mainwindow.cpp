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
#include <QDateTime>
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
#include "debug/print.hpp"
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
#include "DockManager.h"
#include "FloatingDockContainer.h"

#define GROX_HAVE_BITSTAMP
#define GROX_HAVE_XRPL

// ----------------------------------------------------------------------------
extern void generate_encrypted_ini_data(password_dialog& npw);

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
template <int Level>
inline constexpr print_threshold<Level, 2> main_dbg("Main-win");

using namespace ads;

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
  CDockWidget* PlotDockWidget = new CDockWidget(to_qstring(title));
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
  CDockWidget* obPlotDockWidget = new CDockWidget(to_qstring(obtitle));
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
  CDockWidget* obpDockWidget = new CDockWidget(to_qstring(obptitle));
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
      main_dbg<4>.debug(ffmt<s20>("Orderbook-Text"), "orderbook_plot_sub");
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
      main_dbg<4>.debug(ffmt<s20>("Orderbook-Plot"), "orderbook_plot_sub");
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
  main_dbg<0>.debug(ffmt<s20>("Stream"), "factory_create");
  if (stream == ticker::streams::price_data)
    create_ticker_price_plot(tdata, cp);
  else if (stream == ticker::streams::order_book)
    create_ticker_orderbook_widgets(tdata, cp);
  else
    main_dbg<0>.error(
        ffmt<s20>("Stream"), "factory_create no GUI for stream", ticker::stream_names[stream]);
}

// ----------------------------------------------------------------------------
void ticker_stream_gui_destructor(currency_pair cp, ticker::data tdata, ticker::streams stream)
{
  main_dbg<0>.debug(ffmt<s20>("Stream"), "factory_destroy");
  if (stream == ticker::streams::price_data)
  {
    tdata->chart_widget_->parentWidget()->deleteLater();
    tdata->chart_widget_.reset();
    tdata->live_trade_subscribers_.clear();
    tdata->view_.reset();
    main_dbg<0>.error(ffmt<s20>("Stream"), tdata->chart_widget_.use_count());
    main_dbg<0>.error(ffmt<s20>("Stream"), tdata->view_.use_count());
  }
  else if (stream == ticker::streams::order_book)
  {
    //    tdata->orderbook_->parentWidget()->deleteLater();
    tdata->orderbook_.reset();
  }
  else
    main_dbg<0>.error(ffmt<s20>("Stream"), "factory_destroy unknown stream");
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
  CDockWidget* NetworkDockWidget = new CDockWidget("Networks");
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
  CDockWidget* AccountsDockWidget = new CDockWidget("Accounts");
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
  CDockWidget* OrdersDockWidget = new CDockWidget("Trades");
  OrdersDockWidget->setWidget(orders_frame_, CDockWidget::AutoScrollArea);
  OrdersDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContentMinimumSize);
  OrdersDockWidget->setMinimumSize(128, 196);
  auto const OrdersautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
      SideBarLocation::SideBarRight, OrdersDockWidget);
  OrdersautoHideContainer->setSize(256);
  global_settings.dockwindows_menu_->addAction(OrdersDockWidget->toggleViewAction());

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
      main_dbg<0>.debug(ffmt<s20>("wallet_widget"), "set_data", wname);
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
    main_dbg<0>.debug(ffmt<s20>("Init abstract_exchange"), e->get_name());
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
  main_dbg<0>.debug(ffmt<s20>("appExitCleanupHandler"));
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
  qs_darkmode_ = new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::Key_D)), this, [this]() {
    dark_mode_ = (dark_mode_ + 1) % 3;
    LoadStyleSheet(dark_mode_);
  });

  qs_password_ =
      new QShortcut(QKeySequence(int(Qt::CTRL) + int(Qt::SHIFT) + int(Qt::Key_P)), this, [this]() {
        main_dbg<6>.debug("Shift click pressed");
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
}

// ----------------------------------------------------------------------------
// @TODO - this should be moved into accounts frame/widget
// slot to ensure widget updates on GUI thread
void GroxMainWindow::wallet_changed(ledger_wallet* w)
{
  std::string wname = fmt::format("{}/{}", w->network_->get_name(), w->name_);
  main_dbg<0>.debug(ffmt<s20>("wallet_changed"), wname);
  // widgets are added to the layout, but are "owned" by the layout's parent
  auto* widget = accounts_frame_->findChild<wallet_widget*>(to_qstring(wname));
  if (widget) { widget->set_data(w); }
  else { main_dbg<0>.error(ffmt<s20>("wallet_changed"), "Failed to locate", wname); }
  display_offers();
}

// ----------------------------------------------------------------------------
void GroxMainWindow::transaction_event()
{
  main_dbg<5>.debug("transaction_event : update balances?");
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
  main_dbg<0>.debug(ffmt<s20>("closeEvent"));

  if (!exchange_list_.empty())
  {
    saveTrustlines();
    saveConnectionSetups();

    auto snd = stdexec::start_on(QtStdExec::QThreadScheduler(), stdexec::just())    //
        | stdexec::then([this]() {
            for (auto& e : exchange_list_)
            {
              auto name = e->get_name();
              main_dbg<0>.debug(ffmt<s20>("shut down"), name, "start");
              e->shut_down();
              e.reset();
              main_dbg<0>.debug(ffmt<s20>("shut down"), name, "complete");
            }
            main_dbg<0>.debug(ffmt<s20>("exchanges"), "shutdown complete");
            exchange_list_.clear();
          })                                                     //
        | stdexec::continue_on(QtStdExec::QThreadScheduler())    // pika -> Qt
        | stdexec::then([this]() {
            main_dbg<0>.debug(ffmt<s20>("Close"));
            close();
          });
    stdexec::start_detached(std::move(snd));
    // we need to asynchronously shut down,
    // close all network connections, these need the application messaging loop
    // to correctly process everything (because they use Qt Networking/threads),
    // so we will ignore the close event, and call 'close' on ourselves once shutdown is ready
    main_dbg<0>.debug(ffmt<s20>("closeEvent"), "Ignore");
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
    auto snd = stdexec::start_on(QtStdExec::QThreadScheduler(), stdexec::just())    //
        | stdexec::then([this]() { loadConnectionSetups(); })                       //
        | stdexec::then([this]() { loadWindowSettings(); });                        //
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
  main_dbg<0>.debug(ffmt<s20>("Trustlines saved"), settings.fileName().toStdString());
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
  main_dbg<0>.debug(ffmt<s20>("Trustlines loaded"), settings.fileName().toStdString());
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
    for (auto const& ticker : e->tickers_subscribed())
    {
      // begin ticker group
      auto cp = ticker.first;
      std::string key = currency_pair_string(cp);
      settings.beginGroup(to_qstring(key));

      // for each stream available
      for (auto const& s : streams)
      {
        auto skey = std::string(magic_enum::enum_name(s));
        main_dbg<6>.debug(ffmt<s20>("saveConnectionSetups"), "Stream subscribed", key, skey);
        bool subscribed = e->is_stream_subscribed(cp, s);
        settings.setValue(skey.c_str(), subscribed);
        if (subscribed)
          main_dbg<0>.debug(ffmt<s20>("saveConnectionSetups"), "Stream subscribed",
              settings.group().toStdString(), key);
      }
      settings.endGroup();    // ticker
    }
    settings.endGroup();    // abstract_exchange
  }
  settings.endGroup();    // streams
  main_dbg<0>.debug(ffmt<s20>("Connections saved"), settings.fileName().toStdString());
}

// ----------------------------------------------------------------------------
void GroxMainWindow::loadConnectionSetups()
{
  QSettings settings(global_settings.iniFileName, QSettings::IniFormat);

  main_dbg<0>.error(ffmt<s20>("Fix connect init"), settings.fileName().toStdString());
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

  main_dbg<0>.debug(ffmt<s20>("Settings saved"), settings.fileName().toStdString());
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

  main_dbg<0>.debug(ffmt<s20>("Settings loaded"), settings.fileName().toStdString());
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
    return;
  }
  else if (dark == 1) { name = ":qdarkstyle/dark/darkstyle.qss"; }
  else if (dark == 2) { name = ":qdarkstyle/light/lightstyle.qss"; }
  QFile f(name);
  if (!f.exists())
  {
    main_dbg<0>.error(ffmt<s20>("Stylesheet"), "Unable to set stylesheet, file not found");
  }
  else
  {
    f.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&f);
    global_settings.dock_manager_->setStyleSheet("");
    qApp->setStyleSheet(ts.readAll());
  }
}
