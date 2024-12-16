#include <memory>

#include "config/config.hpp"
#include "currency/currency.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "exchange/exchange.hpp"
#include "widgets/arbitrage_widget.hpp"
//
// Qt Advanced Docking System
#include "AutoHideDockContainer.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "DockManager.h"
#include "FloatingDockContainer.h"
// generated
#include "ui_arbitrage_widget.h"

using namespace ads;

// ----------------------------------------------------------------------------
std::shared_ptr<arbitrage_widget> create_arbitrage_widget(exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<arbitrage_widget> algowidget_ =
      std::make_shared<arbitrage_widget>(nullptr);    // , view, tdata->exchange_, cp);

  // put the price plot into a dock widget
  // using namespace ads;
  // std::string title = currency_pair_string(cp) + " price " + tdata->exchange_->get_name();
  // CDockWidget* PlotDockWidget = new CDockWidget(to_qstring(title));
  // PlotDockWidget->setWidget(chart_widget.get());
  // PlotDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  // global_settings.dock_manager_->addDockWidgetFloating(PlotDockWidget);
  // global_settings.dockwindows_menu_->addAction(PlotDockWidget->toggleViewAction());

  // ----------------------------------
  // gui object for/with arbitrage controls
  Ui::arbitrage_widget* ui_ = new Ui::arbitrage_widget();
  ui_->setupUi(algowidget_.get());
  // algowidget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  // indicators::indicator_vector algo;
  // indicator_widget* iw = new indicator_widget(algo);

  // ----------------------------------
  // Create dockwidget to hold our controls
  CDockWidget* AlgorithmsDockWidget = new CDockWidget("Algorithms");
  AlgorithmsDockWidget->setWidget(algowidget_.get());
  AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
  AlgorithmsDockWidget->setMinimumSize(128, 196);
  auto const AlgorithmsautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
      SideBarLocation::SideBarRight, AlgorithmsDockWidget);
  AlgorithmsautoHideContainer->setSize(256);
  // global_settings.dock_manager_->addDockWidgetFloating(PlotDockWidget);
  global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

  return algowidget_;
}

// ----------------------------------------------------------------------------
// void GroxMainWindow::perform_arbitrage()
// {
//   double budget = 100000;
//   std::string arbitrage_string;
//   double test_offset = 0.00;
//   if (algo_form_->arbitrage_test_mode->isChecked())
//   {
//     try
//     {
//       test_offset = std::stod(algo_form_->arbitrage_test_offset->text().toStdString());
//     }
//     catch (...)
//     {
//       test_offset = 0.00;
//     }
//   }

//   //
//   fee_data sell_fee{0.12, 0.0};
//   fee_data buy_fee{0.0, 0.01};
//   //
//   if (algo_form_->enable_arbitrage->isChecked())
//   {
//     // @TODO fix arbitrage for CP
//     auto& xrp_orderbook = xrpl_network_->get_orderbook({{"", "XRP"}, {"", "USD"}});
//     auto& bit_orderbook = bitstamp_network_->get_orderbook({{"", "XRP"}, {"", "USD"}});
//     xrp_orderbook.compute_arbitrage(
//         bit_orderbook, budget, buy_fee, sell_fee, test_offset, arbitrage_string);

//     if (arbitrage_string.size() > 0)
//     {
//       QString arb_string = QString::fromStdString(arbitrage_string);
//       algo_form_->arbitrage_orders->setPlainText(arb_string);
//     }
//     else { algo_form_->arbitrage_orders->setPlainText(""); }
//   }
// }
