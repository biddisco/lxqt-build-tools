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
#include "widgets/trade_algorithm_widget.hpp"
#include "widgets_control/control_builder.hpp"
#include "widgets_control/control_factory.hpp"
#include "widgets_control/indicator_widget.hpp"
// generated
#include "ui_trade_algorithm_arbitrage.h"
#include "ui_trade_algorithm_currency_exchange.h"
#include "ui_trade_algorithm_market_maker.h"

using namespace ads;

// ----------------------------------------------------------------------------
// Sets the height of a text widget to be a nice multiple of row height
// based on the font used and other settings like mergins/borders etc
void setHeight(QPlainTextEdit* ptxt, int nRows)
{
  QTextDocument* pdoc = ptxt->document();
  QFontMetrics fm(pdoc->defaultFont());
  QMargins margins = ptxt->contentsMargins();
  int nHeight = ((1 + fm.lineSpacing()) * nRows) +
      ((pdoc->documentMargin() + ptxt->frameWidth()) * 2) +    //
      margins.top() + margins.bottom();
  ptxt->setFixedHeight(nHeight);
}

// ----------------------------------------------------------------------------
// convenience function that takes an order book parameter and sets gui labels
// up with the correct currency/exchange info and returns the ticker data
// associated with the param
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
  ticker::data tdata;
  try
  {
    tdata = (*it)->get_subscribed_ticker_data(cp);
  }
  catch (...)
  {
    //SPDLOG_ERROR("Unable to create orderbook due to unsubscribed ticker");
  }
  //
  return tdata;
}

// ----------------------------------------------------------------------------
QDialog* gui_trade_currency_exchange(
    indicators::shared_algorithm alg, abstract_exchange::exchange_vector exchange_list_)
{
  QDialog* algowidget_ = new QDialog(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

  // instantiate the gui object for/with currency_exchange controls
  Ui::trade_algorithm_currency_exchange* ui_ = new Ui::trade_algorithm_currency_exchange();
  ui_->setupUi(algowidget_);

  // setup orderbook text displays (x2 for currency from and to)
  size_t const font_size = 8;
#ifdef RESIZE_TEXTBOX_TO_LIMIT
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
  setHeight(ui_->actions_txt, 10);

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

  // lamda that updates values in the gui whenever orderbook data changes
  auto arb_lambda = [ui_](ticker::data tdata1, ticker::data tdata2, auto* obwidget1,
                        auto* obwidget2) {
    double budget = std::stod(ui_->spend->text().toStdString());
    //
    // fee_data maker_fee{ui_->maker_fee->value(), 0.00};
    fee_data taker_fee{ui_->taker_fee->value(), 0.00};

    // convert budget in source currency into equivalent xrp using orderbook
    std::string buy_output, sell_output;
    auto buys = tdata1->orderbook_->compute_buy_amount(budget, taker_fee, buy_output);
    obwidget1->setPlainText(to_qstring(buy_output));

    // convert xrp amount into dest currency using orderbook
    auto sells = tdata2->orderbook_->compute_sell_amount(buys.amount, taker_fee, sell_output);
    obwidget2->setPlainText(to_qstring(sell_output));

    // update
    ui_->partial->setText(to_qstring(fmt::format("{:11.4f} ", buys.amount)));
    ui_->receive->setText(to_qstring(fmt::format("{:11.4f} ", sells.amount)));
    ui_->rate->setText(to_qstring(fmt::format("{:11.4f} ", sells.amount / budget)));
  };

  // bind values to previous lambda and return a new lambda with everything setup
  // to call that lambda on the application thread
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

  // attach gui update lambda to the orderbooks
  tdata1->orderbook_subscribers_.subscribe(
      "currency1", makeLambda(tdata1, tdata2, ui_->orderbook1, ui_->orderbook2));
  tdata2->orderbook_subscribers_.subscribe(
      "currency2", makeLambda(tdata1, tdata2, ui_->orderbook1, ui_->orderbook2));

  QObject::connect(ui_->execute_btn, &QPushButton::clicked, [=](bool b) {
    double budget = std::stod(ui_->spend->text().toStdString());
    //
    std::string text = fmt::format("Executing trade {} {}", budget, c1.c2_.code_);
    ui_->actions_txt->appendPlainText(to_qstring(text));
    //
    std::vector<trade_data> trades;
    //
    // std::shared_ptr<abstract_exchange> network_;
    // std::string wallet_;
    // currency_code taker_payc_;
    // currency_code taker_getc_;
    // double taker_pay_;
    // double taker_get_;
    // double exchange_rate_;
    // double fee_percent_;
    // double fee_fixed_;
    // std::uint64_t id_;
    // std::string datetime_;
    // bool confirmed_;

    // trade_data t{
    //     tdata1->exchange_,
    //     account_->name_,
    //     taker_payc,                 // taker pays this currency
    //     this->currency_.symbol_,    // taker gets this currency
    //     taker_pay,                  // taker pays this amount (total)
    //     taker_get,                  // taker gets this amount (total)
    //     price,                      // abstract_exchange rate : TODO - check fee settings
    //     fee_percent,                // abstract_exchange rate : TODO - check fee settings
    //     fee_percent,                // abstract_exchange rate : TODO - check fee settings
    //     0,                          // Id
    //     now.toStdString(),
    //     false,
    // };
    // trades.push_back(t);
    // tdata1->exchange_->
  });

  return algowidget_;
}

// ----------------------------------------------------------------------------
QDialog* gui_trade_market_maker(
    indicators::shared_algorithm alg, abstract_exchange::exchange_vector exchange_list_)
{
  QDialog* algowidget_ = new QDialog(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

  // gui object for/with market_maker controls
  Ui::trade_algorithm_market_maker* ui_ = new Ui::trade_algorithm_market_maker();
  ui_->setupUi(algowidget_);

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
QDialog* gui_trade_arbitrage(
    indicators::shared_algorithm alg, abstract_exchange::exchange_vector exchange_list_)
{
  QDialog* algowidget_ = new QDialog(nullptr);
  algowidget_->setProperty("DockPos", QVariant(static_cast<SideBarLocation>(SideBarLeft)));

  // gui object for/with market_maker controls
  Ui::trade_algorithm_arbitrage* ui_ = new Ui::trade_algorithm_arbitrage();
  ui_->setupUi(algowidget_);

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

  if (tdata1 && tdata2)
  {
    tdata1->orderbook_subscribers_.subscribe("arbitrage1", makeLambda(tdata1, ui_->orderbook1));
    tdata2->orderbook_subscribers_.subscribe("arbitrage2", makeLambda(tdata2, ui_->orderbook2));
  }
  else {}
  return algowidget_;
}

// ----------------------------------------------------------------------------
// given an algorithm : return a widget that represents it, populated with controls
// for ticker information and options etc etc
QDialog* trade_widget_factory(
    indicators::shared_algorithm alg, abstract_exchange::exchange_vector exchange_list_)
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
QDialog* dock_trading_widget(QDialog* algowidget, std::string name)
{
  // ----------------------------------
  // Create dockwidget to hold our controls
  CDockWidget* AlgorithmsDockWidget = new CDockWidget(global_settings.dock_manager_, name.c_str());
  AlgorithmsDockWidget->setWidget(algowidget, CDockWidget::AutoScrollArea);
  // AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromContent);
  AlgorithmsDockWidget->setMinimumSize(128, 196);
  SideBarLocation sidebarloc = algowidget->property("DockPos").value<SideBarLocation>();
  auto const AlgorithmsautoHideContainer =
      global_settings.dock_manager_->addAutoHideDockWidget(sidebarloc, AlgorithmsDockWidget);
  AlgorithmsautoHideContainer->setSize(256);
  global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

  return algowidget;
}

// ----------------------------------------------------------------------------
