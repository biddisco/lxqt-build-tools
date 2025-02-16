#include <memory>
#include <string>
//
#include <fmt/format.h>
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
#include "exchange/abstract_exchange.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "indicators/trade_currency_exchange.hpp"
#include "indicators/trade_market_maker.hpp"
#include "util/stringutils.hpp"
#include "widgets/indicator_widget.hpp"
#include "widgets/trade_algorithm_widget.hpp"
// generated
#include "ui_trade_algorithm_arbitrage.h"
#include "ui_trade_algorithm_currency_exchange.h"
#include "ui_trade_algorithm_market_maker.h"

using namespace ads;

// ----------------------------------------------------------------------------
ticker::data set_gui_orderbook(order_book_param const& p, int index, QLabel* l1, QLabel* l2)
{
  std::string name = p.exchange_;
  assert(index < p.tickers_.size());
  currency_pair cp = p.tickers_[index];
  //
  l1->setText(name.c_str());
  l2->setText(currency_pair_qstring(cp));
  //
  auto it = std::find_if(global_settings.networks_.begin(), global_settings.networks_.end(),
      [name](auto n) { return n->get_name() == name; });
  auto tdata = (*it)->get_subscribed_ticker_data(cp);
  //
  return tdata;
}

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> gui_trade_currency_exchange(
    std::shared_ptr<indicators::algorithm_base> alg,
    abstract_exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<QDialog> algowidget_ = std::make_shared<QDialog>(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

  // gui object for/with currency_exchange controls
  Ui::trade_algorithm_currency_exchange* ui_ = new Ui::trade_algorithm_currency_exchange();
  ui_->setupUi(algowidget_.get());

  // fill orderbook text display
  size_t const font_size = 8;
#ifdef RESIZE_TO_LIMIT
  QString txt = "X";
  int char_size = QFontMetrics(ui_->orderbook1->font()).horizontalAdvance(txt);
  int calcWidth = char_size * 85 + 8;
  ui_->orderbook1->setMinimumWidth(calcWidth);
#endif
  QFont font = QFont();
  font.setPointSize(font_size);
  font.setFamily("Courier");
  ui_->orderbook1->setFont(font);
  ui_->orderbook2->setFont(font);

  order_book_param p1 = indicators::get<order_book_param>(alg->get_params(), 0);
  currency_pair c1 = p1.tickers_[0];
  currency_pair c2 = p1.tickers_[1];
  ui_->C1->setText(to_qstring(c1.c2_.code_));
  ui_->I1->setText(to_qstring(c1.c1_.code_));
  ui_->C2->setText(to_qstring(c2.c2_.code_));
  ticker::data tdata1 = set_gui_orderbook(p1, 0, ui_->exchange1, ui_->ticker1);
  ticker::data tdata2 = set_gui_orderbook(p1, 1, ui_->exchange2, ui_->ticker2);
  ui_->taker_fee->setValue(tdata1->exchange_->get_fees(c1).taker_percent);
  ui_->maker_fee->setValue(tdata1->exchange_->get_fees(c2).maker_percent);

  auto arb_lambda = [ui_](ticker::data tdata1, ticker::data tdata2, auto* obwidget1,
                        auto* obwidget2) {
    double budget = std::stod(ui_->spend->text().toStdString());
    //
    fee_data sell_fee{0.12, 0.0};
    fee_data taker_fee{ui_->taker_fee->value(), 0.01};
    //
    std::string buy_output, sell_output;
    auto buys = tdata1->orderbook_->compute_buy_amount(budget, taker_fee, buy_output);
    obwidget1->setPlainText(to_qstring(buy_output));
    auto sells = tdata2->orderbook_->compute_sell_amount(buys.amount, taker_fee, sell_output);
    obwidget2->setPlainText(to_qstring(sell_output));
    ui_->partial->setText(to_qstring(fmt::format("{:11.4f} ", buys.amount)));
    ui_->receive->setText(to_qstring(fmt::format("{:11.4f} ", sells.amount)));
    ui_->rate->setText(to_qstring(fmt::format("{:11.4f} ", sells.amount / budget)));
  };

  auto makeLambda = [arb_lambda](ticker::data tdata1, ticker::data tdata2, auto* obwidget1,
                        auto* obwidget2) {
    return [tdata1, tdata2, obwidget1, obwidget2, arb_lambda](currency_pair cp) {
      QMetaObject::invokeMethod(QCoreApplication::instance()->thread(), [=]() {
        if (tdata1 && tdata1->orderbook_ && tdata2 && tdata2->orderbook_)
        {
          arb_lambda(tdata1, tdata2, obwidget1, obwidget2);
        }
      });
    };
  };

  tdata1->orderbook_subscribers_.subscribe(
      "arbitrage1", makeLambda(tdata1, tdata2, ui_->orderbook1, ui_->orderbook2));
  // tdata2->orderbook_subscribers_.subscribe("arbitrage2", makeLambda(tdata2, ui_->orderbook2));

  return algowidget_;
}

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> gui_trade_market_maker(std::shared_ptr<indicators::algorithm_base> alg,
    abstract_exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<QDialog> algowidget_ = std::make_shared<QDialog>(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

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

  auto tdata1 = set_gui_orderbook(
      indicators::get<order_book_param>(alg->get_params(), 0), 0, ui_->exchange1, ui_->ticker1);

  auto makeLambda = [](ticker::data tdata, auto* obwidget) {
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
std::shared_ptr<QDialog> gui_trade_arbitrage(std::shared_ptr<indicators::algorithm_base> alg,
    abstract_exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<QDialog> algowidget_ = std::make_shared<QDialog>(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

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

  auto tdata1 = set_gui_orderbook(
      indicators::get<order_book_param>(alg->get_params(), 0), 0, ui_->exchange1, ui_->ticker1);
  auto tdata2 = set_gui_orderbook(
      indicators::get<order_book_param>(alg->get_params(), 1), 0, ui_->exchange2, ui_->ticker2);

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

  auto makeLambda = [arb_lambda](ticker::data tdata, auto* obwidget) {
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

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> trade_widget_factory(std::shared_ptr<indicators::algorithm_base> alg,
    abstract_exchange::exchange_vector exchange_list_)
{
  if (dynamic_pointer_cast<indicators::trade_currency_exchange>(alg))
  {
    return gui_trade_currency_exchange(alg, exchange_list_);
  }
  else if (dynamic_pointer_cast<indicators::trade_market_maker>(alg))
  {
    return gui_trade_market_maker(alg, exchange_list_);    //
  }
  else if (dynamic_pointer_cast<indicators::trade_arbitrage_2_way>(alg))
  {
    return gui_trade_arbitrage(alg, exchange_list_);
  }
  return nullptr;
}

// ----------------------------------------------------------------------------
std::shared_ptr<QDialog> create_trading_widget(abstract_exchange::exchange_vector exchange_list_)
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
    AlgorithmsDockWidget->setWidget(algowidget_.get(), CDockWidget::AutoScrollArea);
    // AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
    AlgorithmsDockWidget->setMinimumSize(128, 196);
    SideBarLocation sidebarloc = algowidget_->property("DockPos").value<SideBarLocation>();
    auto const AlgorithmsautoHideContainer =
        global_settings.dock_manager_->addAutoHideDockWidget(sidebarloc, AlgorithmsDockWidget);
    AlgorithmsautoHideContainer->setSize(256);
    global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

    return algowidget_;
  }
  return nullptr;
}

// ----------------------------------------------------------------------------
