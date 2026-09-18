#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
//
#include <QColorDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
//
#include <QwtPlotCurve>
#include <QwtPlotItem>
//
#include "ui_price_chart_widget.h"
//
#include "config/config.hpp"
#include "debug/demangle_helper.hpp"
#include "debug/logging.hpp"
#include "indicators/indicator_params.hpp"
#include "indicators/indicator_types.hpp"
#include "plot/ohlc_chart_curve.hpp"
#include "plot/timebased_data_curve.hpp"
#include "senders/pika_stdexec.hpp"
#include "senders/qtstdexec.hpp"
#include "senders/start_detached.hpp"
#include "util/stringutils.hpp"
#include "widgets/digital_clock.hpp"
#include "widgets/price_chart_widget.hpp"
#include "widgets_control/indicator_widget.hpp"

// ----------------------------------------------------------------------------
static auto pplot_log = grox::log::create("PricePlt");

// ----------------------------------------------------------------------------
QColor chart_colours[10] = {QColor("cyan"), QColor("magenta"), QColor("red"), QColor("darkRed"),
    QColor("darkCyan"), QColor("darkMagenta"), QColor("green"), QColor("darkGreen"),
    QColor("yellow"), QColor("blue")};

// ----------------------------------------------------------------------------
price_chart_widget::price_chart_widget(QWidget* parent, std::shared_ptr<ohlc_dataset_view> ohlc,
    std::shared_ptr<abstract_exchange> ex, currency_pair cp)
  : QWidget(parent)
  , ui(new Ui::price_chart_widget)
  , hdf5_ohlc_(ohlc)
  , exchange_(ex)
  , ticker_string_(currency_pair_string(cp))
{
  ui->setupUi(this);
  ui->ticker->setText(ticker_string_.data());

  //
  // Create candlestick plot
  //
  price_plot_ = new ohlc_price_plot(this, hdf5_ohlc_);
  ui->candlestick_layout->addWidget(price_plot_);

  //
  // Create indicators table
  //
  QStringList slist("Auto");
  for (auto const& r : ohlc_data_resolutions::available_resolutions()) { slist << r.name_; }
  ui->candle_res->addItems(slist);

  ind_vis_ = new QTableView(this);
  ind_vis_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  ind_vis_->setMinimumHeight(24);
  ind_vis_->setMaximumHeight(24);
  ind_vis_->setModel(&ind_model_);

  QHeaderView* horizontalHeader = ind_vis_->horizontalHeader();
  horizontalHeader->setVisible(false);
  horizontalHeader->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
  horizontalHeader->setSectionResizeMode(1, QHeaderView::ResizeMode::Interactive);
  horizontalHeader->setSectionResizeMode(2, QHeaderView::ResizeMode::ResizeToContents);
  horizontalHeader->setSectionResizeMode(3, QHeaderView::ResizeMode::ResizeToContents);
  horizontalHeader->setSectionResizeMode(4, QHeaderView::ResizeMode::ResizeToContents);

  QHeaderView* verticalHeader = ind_vis_->verticalHeader();
  verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
  verticalHeader->setDefaultSectionSize(24);
  verticalHeader->setVisible(false);
  ui->controls_layout->addWidget(ind_vis_);

  btn_indicator_ = new QPushButton(this);
  btn_indicator_->setText("Indicators");
  btn_indicator_->setFlat(true);
  ui->controls_layout->addWidget(btn_indicator_);
  //
  DigitalClock* clock = new DigitalClock(this, exchange_->get_clock_timer());
  ui->controls_layout->addWidget(clock);
  //
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumSize(128, 96);
  //
  connect_gui();
}

// ----------------------------------------------------------------------------
price_chart_widget::~price_chart_widget()
{
  // Explicitly detach and delete every indicator curve before any plot widget is destroyed.
  // This avoids QwtPlot::~QwtPlot() trying to auto-detach the same curve objects later.
  for (auto& indicator : ind_model_.indicators_)
  {
    for (auto* curve : indicator.curves)
    {
      curve->detach();

      // delete curve;
    }
    indicator.curves.clear();
    indicator.plot = nullptr;
  }

  // Destroy indicator objects while the underlying data sources are still alive so any
  // pub/sub unsubscribe logic runs against valid state.
  ind_model_.indicators_.clear();

  // Plot widgets can now be torn down safely because they no longer own any indicator curves.
  for (auto p : filter_plots_) { delete p; }
  delete price_plot_;
  filter_plots_.clear();

  hdf5_ohlc_.reset();
  exchange_.reset();
  GROX_LOG_DEBUG(pplot_log, "{:>20}", "~price_chart_widget");
  delete ui;
}

// ----------------------------------------------------------------------------
void price_chart_widget::connect_gui()
{
  // Graph resolution buttons
  connect(
      ui->gt_6, &QAbstractButton::clicked, this, [this]() { graph_rescale(-2); },
      Qt::QueuedConnection);
  connect(
      ui->gt_12, &QAbstractButton::clicked, this, [this]() { graph_rescale(-1); },
      Qt::QueuedConnection);
  connect(
      ui->gt_d, &QAbstractButton::clicked, this, [this]() { graph_rescale(0); },
      Qt::QueuedConnection);
  connect(
      ui->gt_w, &QAbstractButton::clicked, this, [this]() { graph_rescale(1); },
      Qt::QueuedConnection);
  connect(
      ui->gt_m, &QAbstractButton::clicked, this, [this]() { graph_rescale(2); },
      Qt::QueuedConnection);
  connect(
      ui->gt_y, &QAbstractButton::clicked, this, [this]() { graph_rescale(3); },
      Qt::QueuedConnection);
  connect(
      ui->gt_a, &QAbstractButton::clicked, this, [this]() { graph_rescale(4); },
      Qt::QueuedConnection);

  // change candle resolution combo
  connect(
      ui->candle_res, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      [this](int index) {
        double res = 0;
        price_plot_->set_auto_candle_resolution(index == 0);
        if (index > 0) { res = ohlc_data_resolutions::available_resolutions()[index - 1]; }
        if (price_plot_->adjust_candle_size(res))
        {    // candles changed, so recompute volume range {min,max}
          price_plot_->adjust_data_scaling();
        }
        price_plot_->replot();
      },
      Qt::QueuedConnection);

  // Heikin-Ashi candle type checkbox
  connect(
      ui->heikin, QOverload<Qt::CheckState>::of(&QCheckBox::checkStateChanged), this,
      [this](Qt::CheckState state) {
        if (state) { price_plot_->setMode(ohlc_chart_curve::HeikinAshi); }
        else { price_plot_->setMode(QwtPlotTradingCurve::SymbolStyle::CandleStick); }
      },
      Qt::QueuedConnection);

  // Delete data using R keypress
  connect(
      price_plot_->get_interactor(), &ohlc_interactor::repair_pressed, this,
      [this](QPointF p) {
        auto crosshairs = price_plot_->get_crosshairs();
        double msecs = crosshairs->quantize_x_coord(p.x());
        QDateTime const dt = QDateTime::fromMSecsSinceEpoch(msecs);
        QString s = QLocale::system().toString(dt, "dd-MM-yy hh:mm");

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(
            this, "Confirm", "Delete from " + s, QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
          hdf5_ohlc_->truncate_from_time(msecs);
          GROX_LOG_ERROR(pplot_log, "{:>20}", "emit update_candlestick_data");
          this->replot();
          // update_candlestick_data();
        }
        else { GROX_LOG_DEBUG(pplot_log, "{:>20}", "Yes *not* clicked"); }
      },
      Qt::QueuedConnection);

  // underlying data plot time axis changed
  connect(
      price_plot_, &ohlc_price_plot::timeAxisChanged, this,
      [this](double t1, double t2) {
        for (auto p : filter_plots_) { p->update_time_axis(t1, t2, false); }
      },
      Qt::QueuedConnection);

  // crosshairs moving - replot indicators etc
  connect(
      price_plot_->get_crosshairs(), &ohlc_picker::moved, this,
      [this](QPointF const& pos) {
        // coordinates received are in time/price(other) units
        // so no need to remap the time axis before sending
        for (auto p : filter_plots_) { p->onCrossHairsMoved(pos); }
      },
      Qt::QueuedConnection);

  connect(ind_vis_, &QTableView::clicked, this, [this, table = ind_vis_](QModelIndex const& i) {
    int col = i.column();
    int row = i.row();
    if (col == 2)
    {
      auto it = std::next(ind_model_.indicators_.begin(), row);
      for (auto* curve : it->curves)
      {
        QColor c = curve->pen().color();
        QColor color = QColorDialog::getColor(c, this);
        if (color.isValid())
        {
          QPen new_pen(curve->pen());
          new_pen.setColor(color);
          curve->setPen(new_pen);
        }
      }
      ind_model_.dataAdded();
      this->replot();
    }
    else if (col == 3)    // toggle indicator curve visibility
    {
      auto it = std::next(ind_model_.indicators_.begin(), row);
      it->visibility_ = !it->visibility_;
      for (auto* curve : it->curves)
      {
        if (it->visibility_)
          curve->show();
        else
          curve->hide();
      }
      this->replot();
      // clear selection
      table->setCurrentIndex(QModelIndex());
    }
    else if (col == 4)    // delete indicator
    {
      auto it = std::next(ind_model_.indicators_.begin(), row);
      for (std::size_t i = 0; i < it->curves.size(); ++i)
      {
        remove_indicator_plot(it->curve_plots[i], it->curves[i]);
      }
      ind_model_.indicators_.erase(it);
      ind_model_.dataAdded();
      this->replot();
    }
  });

  static std::size_t available_indicators_index{0};

  connect(btn_indicator_, &QPushButton::clicked, this, [this](bool b) {
    GROX_LOG_DEBUG(pplot_log, "{:>20} {}", "Indicators", ticker_string_);

    using namespace grox::senders;

    auto const& graph_indicators =
        indicators::indicator_registry::getInstance().by_kind(indicators::indicator_kind::graph);
    indicator_widget* widget = new indicator_widget(&graph_indicators, available_indicators_index);
    auto result = widget->execute_as_dialog();

    if (result == QDialog::Accepted)
    {
      static int colour_count = 0;
      auto snd = stdexec::starts_on(default_pool_scheduler(), stdexec::just())    //
          | stdexec::then([this, widget]() {
              // do this on a pika thread as it executes the algorithm
              indicators::indicator_ptr algp(widget->get_algorithm(), hdf5_ohlc_);
              return algp;
            })                                                      //
          | stdexec::continues_on(QtStdExec::QThreadScheduler())    //
          | stdexec::then([this](indicators::indicator_ptr algp) {
              QString name = QString(algp.indicator()->get_name().c_str());

              for (int i = 0; i < algp.indicator()->num_outputs(); ++i)
              {
                auto colour = chart_colours[colour_count++ % 10];
                auto ot = algp.indicator()->get_overlay(i);
                QwtPlotCurve* curve;
                QwtPlot* curve_plot = nullptr;
                if (ot == indicators::overlay_type::price)
                {
                  curve = price_plot_->add_overlay_curve(
                      name, algp.indicator()->get_outputs()[i].get(), colour);
                  curve_plot = static_cast<QwtPlot*>(price_plot_);
                }
                else if (ot == indicators::overlay_type::buy_sell)
                {
                  if (i == 0)
                    curve = price_plot_->add_buy_sell_curve(
                        "Buy", algp.indicator()->get_outputs()[i]->samples(), Qt::green);
                  else if (i == 1)
                    curve = price_plot_->add_buy_sell_curve(
                        "Sell", algp.indicator()->get_outputs()[i]->samples(), Qt::red);
                  else
                    curve = price_plot_->add_overlay_curve(
                        name, algp.indicator()->get_outputs()[i].get(), colour);
                  curve_plot = static_cast<QwtPlot*>(price_plot_);
                }
                else if (ot == indicators::overlay_type::mode_select)
                {
                  ohlc_modes mode = get<ohlc_modes>(algp.indicator()->get_params(), 2);
                  if (mode == ohlc_modes::volume)
                  {
                    curve = price_plot_->add_overlay_volume_curve(
                        name, algp.indicator()->get_outputs()[i].get(), colour);
                    curve_plot = static_cast<QwtPlot*>(price_plot_);
                  }
                  else if (mode == ohlc_modes::value)
                  {
                    std::tie(algp.plot, curve) =
                        add_indicator_plot(name, algp.indicator()->get_outputs()[i].get(), colour);
                    curve_plot = algp.plot;
                  }
                  else
                  {
                    curve = price_plot_->add_overlay_curve(
                        name, algp.indicator()->get_outputs()[i].get(), colour);
                    curve_plot = static_cast<QwtPlot*>(price_plot_);
                  }
                }
                else if (ot == indicators::overlay_type::shared_axis)
                {
                  if (!algp.plot)
                  {
                    // first shared_axis output creates the plot
                    std::tie(algp.plot, curve) =
                        add_indicator_plot(name, algp.indicator()->get_outputs()[i].get(), colour);
                    curve_plot = algp.plot;
                  }
                  else
                  {
                    // subsequent shared_axis outputs attach to the same plot
                    auto m_curve = new timebased_data_curve(name);
                    m_curve->setYAxis(QwtPlot::yRight);
                    m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
                    m_curve->setStyle(QwtPlotCurve::Lines);
                    m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
                    m_curve->setPen(colour, 2);
                    m_curve->setData(algp.indicator()->get_outputs()[i].get());
                    m_curve->attach(algp.plot);
                    curve = m_curve;
                    curve_plot = algp.plot;
                  }
                }
                else
                {
                  std::tie(algp.plot, curve) =
                      add_indicator_plot(name, algp.indicator()->get_outputs()[i].get(), colour);
                  curve_plot = algp.plot;
                }
                algp.curves.push_back(curve);
                algp.curve_plots.push_back(curve_plot);
              }

              // Qwt curves now own the raw point_chart_data* via setData().
              // Release the shared_ptrs in out_datasets_ to prevent a
              // double-free when the indicator is later destroyed.
              algp.indicator()->release_outputs();

              ind_model_.indicators_.push_back(algp);
              ind_model_.dataAdded();
              this->replot();
            });
      grox::senders::start_detached(std::move(snd), "price_chart_widget add indicator");
    }
  });
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void price_chart_widget::graph_rescale(int range)
{
  auto last_time = hdf5_ohlc_->get_last_sample_time_msec(true);
  double t1 = 0, t2 = last_time;
  if (range == -2) { t1 = last_time - 0.25 * ohlc_data_resolutions::day; }
  else if (range == -1) { t1 = last_time - 0.5 * ohlc_data_resolutions::day; }
  else if (range == 0) { t1 = last_time - 1.0 * ohlc_data_resolutions::day; }
  else if (range == 1) { t1 = last_time - 7 * ohlc_data_resolutions::day; }
  else if (range == 2) { t1 = last_time - 31 * ohlc_data_resolutions::day; }
  else if (range == 3) { t1 = last_time - 365 * ohlc_data_resolutions::day; }
  // special case, to extend current view with new data
  else if (range == 100) { t1 = last_time - 365 * ohlc_data_resolutions::day; }
  else { t1 = hdf5_ohlc_->get_first_sample_time_msec(); }
  price_plot_->update_time_axis(t1, t2, true);
}

// ----------------------------------------------------------------------------
void price_chart_widget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  [[maybe_unused]] bool changed = price_plot_->update_candle_size();
  GROX_LOG_TRACE(pplot_log, "{:>20} {} {}", "Resize", "res changed", changed);
}

// ----------------------------------------------------------------------------
void price_chart_widget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  [[maybe_unused]] bool changed = price_plot_->update_candle_size();
  GROX_LOG_TRACE(pplot_log, "{:>20} {} {}", "Show", "res changed", changed);
}

// ----------------------------------------------------------------------------
void price_chart_widget::show_plot_axes()
{
  // turn on x axis lables for bottom graph (all graphs have same time axis)
  if (filter_plots_.size() == 0)
  {
    price_plot_->enableAxis(QwtPlot::xBottom, true);
    price_plot_->get_crosshairs()->enableDateLabel(true);
  }
  else
  {
    // hide x axis and crosshair date/time label for principal plot
    price_plot_->enableAxis(QwtPlot::xBottom, false);
    price_plot_->get_crosshairs()->enableDateLabel(false);

    // get the last indicator plot and make it's xaxis+label visible
    auto it = filter_plots_.rbegin();
    (*it)->enableAxis(QwtPlot::xBottom, true);
    (*it)->get_crosshairs()->enableDateLabel(true);

    // hide the xaxis+label for other indicators
    for (it++; it != filter_plots_.rend(); ++it)
    {
      (*it)->enableAxis(QwtPlot::xBottom, false);
      (*it)->get_crosshairs()->enableDateLabel(false);
    }
  }
}

// ----------------------------------------------------------------------------
std::tuple<indicator_plot*, timebased_data_curve*> price_chart_widget::add_indicator_plot(
    QString const& title, point_chart_data* data, QColor const& color, indicators::y_limits ylimits)
{
  auto filter_plot = new indicator_plot(this);
  filter_plot->setMinimumHeight(128);
  if (ylimits.min == ylimits.max) { filter_plot->setAxisAutoScale(QwtAxis::YRight, true); }
  else { filter_plot->setAxisScale(QwtAxis::YRight, ylimits.min, ylimits.max); }

  auto m_curve = new timebased_data_curve(title);
  m_curve->setYAxis(QwtPlot::yRight);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::Lines);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
  m_curve->setPen(color, 2);
  m_curve->setData(data);
  m_curve->attach(filter_plot);

  // Align the right axis of the indicator with the main price plot
  auto* scaleWidget = price_plot_->axisWidget(QwtPlot::yRight);
  double extent = scaleWidget->scaleDraw()->extent(scaleWidget->font());
  filter_plot->axisWidget(QwtPlot::yRight)->scaleDraw()->setMinimumExtent(extent);

  // set the initial x min/max rang to tbe the same as the price plot
  auto interval = price_plot_->axisInterval(QwtPlot::xBottom);
  filter_plot->update_time_axis(interval.minValue(), interval.maxValue(), false);
  filter_plots_.push_back(filter_plot);

  // add the plot to the splitter
  ui->graph_splitter->addWidget(filter_plot);
  //  ui->graph_splitter->setPalette(QPalette(QColor("#18191b")));
  ui->graph_splitter->setStyleSheet("QSplitter::handle{background: #18191b; image: none; }");
  filter_plot->show();

  show_plot_axes();

  connect(
      filter_plot, &indicator_plot::timeAxisChanged, this,
      [this](double t1, double t2) { price_plot_->update_time_axis(t1, t2, false); },
      Qt::QueuedConnection);

  return std::make_tuple(filter_plot, m_curve);
}

// ----------------------------------------------------------------------------
void price_chart_widget::remove_indicator_plot(QwtPlot* plot, QwtPlotCurve* curve)
{
  // detach curves and autodelete them
  GROX_LOG_DEBUG(pplot_log, "{:>20} {}", "Remove plot", curve->title().text().toStdString());
  curve->detach();
  delete curve;
  //
  // Only indicator_plot instances are tracked in filter_plots_.
  // Curves on the price plot (overlay/buy-sell) are not in filter_plots_.
  auto* ind_plot = dynamic_cast<indicator_plot*>(plot);
  if (ind_plot)
  {
    filter_plots_.erase(
        std::remove(filter_plots_.begin(), filter_plots_.end(), ind_plot), filter_plots_.end());

    // if there are no curves left, delete the plot and widget, the parent splitter will adjust
    QwtPlotItemList const& items = ind_plot->itemList();
    int num_curves = std::count_if(items.constBegin(), items.constEnd(),
        [](auto const it) { return (it->rtti() == QwtPlotItem::Rtti_PlotCurve); });
    if (num_curves == 0) { delete ind_plot; }
  }
  show_plot_axes();
}
