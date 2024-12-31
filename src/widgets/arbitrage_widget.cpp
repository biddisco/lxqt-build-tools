#include <memory>

#include "config/config.hpp"
#include "currency/currency.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "exchange/exchange.hpp"
#include "indicators/trade_arbitrage_2_way.hpp"
#include "widgets/arbitrage_widget.hpp"
#include "widgets/indicator_widget.hpp"
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

ticker_data init_order_book_params(indicators::param_pair const& p, QLabel* l1, QLabel* l2)
{
  std::string name = std::get<order_book_param>(p.value).exchange_;
  currency_pair cp = std::get<order_book_param>(p.value).ticker_;
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
std::shared_ptr<arbitrage_widget> create_arbitrage_widget(exchange::exchange_vector exchange_list_)
{
  std::shared_ptr<arbitrage_widget> algowidget_ =
      std::make_shared<arbitrage_widget>(nullptr);    // , view, tdata->exchange_, cp);

  // gui object for/with arbitrage controls
  Ui::arbitrage_widget* ui_ = new Ui::arbitrage_widget();
  ui_->setupUi(algowidget_.get());

  QDialog dlg;
  indicator_widget* widget = new indicator_widget(
      indicators::available_arbitragers, indicators::available_arbitragers_index);
  widget->add_to_dialog(&dlg);
  auto result = dlg.exec();
  if (result == QDialog::Accepted)
  {
    auto ap = widget->get_algorithm();
    auto arb = ap->create(ap.get());

    // ----------------------------------
    // Create dockwidget to hold our controls
    CDockWidget* AlgorithmsDockWidget =
        new CDockWidget(global_settings.dock_manager_.get(), "Arbitrage");
    AlgorithmsDockWidget->setWidget(algowidget_.get());
    AlgorithmsDockWidget->setMinimumSizeHintMode(CDockWidget::MinimumSizeHintFromDockWidget);
    AlgorithmsDockWidget->setMinimumSize(128, 196);
    auto const AlgorithmsautoHideContainer = global_settings.dock_manager_->addAutoHideDockWidget(
        SideBarLocation::SideBarTop, AlgorithmsDockWidget);
    AlgorithmsautoHideContainer->setSize(256);
    // global_settings.dock_manager_->addDockWidgetFloating(PlotDockWidget);
    global_settings.dockwindows_menu_->addAction(AlgorithmsDockWidget->toggleViewAction());

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

    auto tdata1 = init_order_book_params(arb->get_params()[0], ui_->exchange1, ui_->ticker1);
    auto tdata2 = init_order_book_params(arb->get_params()[1], ui_->exchange2, ui_->ticker2);

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
  return nullptr;
}

// ----------------------------------------------------------------------------
