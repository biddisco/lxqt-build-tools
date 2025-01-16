#include <memory>
#include <string>
//
// Qt Advanced Docking System
#include "AutoHideDockContainer.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "DockManager.h"
#include "FloatingDockContainer.h"
//
#include "config/config.hpp"
#include "currency/currency.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "exchange/exchange.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "indicators/trade_market_maker.hpp"
#include "widgets/indicator_widget.hpp"
#include "widgets/trade_algorithm_widget.hpp"
// generated
#include "ui_trade_algorithm_arbitrage.h"
#include "ui_trade_algorithm_market_maker.h"

using namespace ads;

// ----------------------------------------------------------------------------
extern ticker_data init_order_book_params(order_book_param const& p, QLabel* l1, QLabel* l2);

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> gui_trade_market_maker(
    std::shared_ptr<indicators::algorithm_base> alg, exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<QDialog> algowidget_ = std::make_shared<QDialog>(nullptr);

  // gui object for/with market_maker controls
  Ui::trade_algorithm_market_maker* ui_ = new Ui::trade_algorithm_market_maker();
  ui_->setupUi(algowidget_.get());

  // fill orderbook text display
  size_t const font_size = 8;
  //auto* orderbook_text = new QPlainTextEdit(nullptr);
  QString txt = "X";
  int char_size = QFontMetrics(ui_->orderbook1->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  ui_->orderbook1->setMinimumWidth(calcWidth);
  QFont font = QFont();
  font.setPointSize(font_size);
  font.setFamily("Courier");
  ui_->orderbook1->setFont(font);

  auto tdata1 = init_order_book_params(
      indicators::get<order_book_param>(alg->get_params(), 0), ui_->exchange1, ui_->ticker1);

  auto makeLambda = [](ticker_data tdata, auto* obwidget) {
    return [tdata, obwidget](currency_pair cp) {
      QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
        if (tdata && tdata->orderbook_)
        {
          QString datastring = QString::fromStdString(tdata->orderbook_->get_orderbook_string());
          obwidget->setPlainText(datastring);
          // arb_lambda();
        }
      });
    };
  };

  tdata1->orderbook_subscribers_.subscribe("market_maker1", makeLambda(tdata1, ui_->orderbook1));
  return algowidget_;
}

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> gui_trade_arbitrage(
    std::shared_ptr<indicators::algorithm_base> alg, exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<QDialog> algowidget_ = std::make_shared<QDialog>(nullptr);

  // gui object for/with market_maker controls
  Ui::trade_algorithm_arbitrage* ui_ = new Ui::trade_algorithm_arbitrage();
  ui_->setupUi(algowidget_.get());

  // fill orderbook text display
  size_t const font_size = 8;
  //auto* orderbook_text = new QPlainTextEdit(nullptr);
  QString txt = "X";
  int char_size = QFontMetrics(ui_->orderbook1->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  ui_->orderbook1->setMinimumWidth(calcWidth);
  QFont font = QFont();
  font.setPointSize(font_size);
  font.setFamily("Courier");
  ui_->orderbook1->setFont(font);
  ui_->orderbook2->setFont(font);
  ui_->arbitrage_orders->setFont(font);

  auto tdata1 = init_order_book_params(
      indicators::get<order_book_param>(alg->get_params(), 0), ui_->exchange1, ui_->ticker1);
  auto tdata2 = init_order_book_params(
      indicators::get<order_book_param>(alg->get_params(), 1), ui_->exchange2, ui_->ticker2);

  auto arb_lambda = [en = ui_->enable_arbitrage, cb = ui_->arbitrage_test_mode,
                        lb = ui_->arbitrage_test_offset, tb = ui_->arbitrage_orders, tdata1,
                        tdata2]() {
    double budget = 100000;
    std::string arbitrage_string;
    double test_offset = 0.00;
    if (cb->isChecked())
    {
      try
      {
        test_offset = std::stod(lb->text().toStdString());
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
    if (en->isChecked())
    {
      // @TODO fix arbitrage for CP
      tdata1->orderbook_->compute_arbitrage(
          *(tdata2->orderbook_), budget, buy_fee, sell_fee, test_offset, arbitrage_string);

      if (arbitrage_string.size() > 0)
      {
        QString arb_string = QString::fromStdString(arbitrage_string);
        tb->setPlainText(arb_string);
      }
      else { tb->setPlainText(""); }
    }
  };

  auto makeLambda = [arb_lambda](ticker_data tdata, auto* obwidget) {
    return [tdata, obwidget, arb_lambda](currency_pair cp) {
      QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
        if (tdata && tdata->orderbook_)
        {
          QString datastring = QString::fromStdString(tdata->orderbook_->get_orderbook_string());
          obwidget->setPlainText(datastring);
          arb_lambda();
        }
      });
    };
  };

  tdata1->orderbook_subscribers_.subscribe("arbitrage1", makeLambda(tdata1, ui_->orderbook1));
  tdata2->orderbook_subscribers_.subscribe("arbitrage2", makeLambda(tdata2, ui_->orderbook2));

  return algowidget_;
}

std::shared_ptr<QDialog> trade_widget_factory(
    std::shared_ptr<indicators::algorithm_base> alg, exchange::exchange_vector exchange_list_)
{
  if (dynamic_pointer_cast<indicators::trade_market_maker>(alg))
  {
    return gui_trade_market_maker(alg, exchange_list_);    //
  }
  else if (dynamic_pointer_cast<indicators::trade_arbitrage_2_way>(alg))
  {    //
    return gui_trade_arbitrage(alg, exchange_list_);
  }
  return nullptr;    // return create_arbitrage_widget();
}
// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> create_trading_widget(exchange::exchange_vector exchange_list_)
{
  QDialog dlg;
  indicator_widget* widget = new indicator_widget(
      indicators::available_arbitragers, indicators::available_arbitragers_index);
  widget->add_to_dialog(&dlg);
  auto result = dlg.exec();
  if (result == QDialog::Accepted)
  {
    auto ap = widget->get_algorithm();
    auto alg = ap->create(ap.get());
    auto algowidget_ = trade_widget_factory(alg, exchange_list_);

    // ----------------------------------
    // Create dockwidget to hold our controls
    CDockWidget* AlgorithmsDockWidget =
        new CDockWidget(global_settings.dock_manager_.get(), alg->get_name().c_str());
    AlgorithmsDockWidget->setWidget(algowidget_.get());
    AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
    AlgorithmsDockWidget->setMinimumSize(128, 196);
    auto const AlgorithmsautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
        SideBarLocation::SideBarTop, AlgorithmsDockWidget);
    AlgorithmsautoHideContainer->setSize(256);
    // global_settings.dock_manager_->addDockWidgetFloating(PlotDockWidget);
    global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

    return algowidget_;
  }
  return nullptr;
}

// ----------------------------------------------------------------------------
