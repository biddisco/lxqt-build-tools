#include <QColorDialog>
#include <QCommonStyle>
#include <QHeaderView>
#include <QMessageBox>
//
#include "ui_price_chart_widget.h"
//
#include "data/ohlc_dataset_manager.hpp"
#include "indicators/indicator_definitions.hpp"
#include "widgets/digital_clock.hpp"
#include "widgets/indicator_dialog.hpp"
#include "widgets/price_chart_widget.hpp"
//
#include "debug/print.hpp"
#include "settings.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of zero disables messages with a priority>0
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> pplot_dbg("PricePlt");

// ----------------------------------------------------------------------------
price_chart_widget::price_chart_widget(QWidget* parent, std::shared_ptr<ohlc_dataset_view> ohlc,
  std::shared_ptr<exchange> ex, std::string ticker)
  : QWidget(parent)
  , ui(new Ui::price_chart_widget)
  , exchange_(ex)
  , hdf5_ohlc_(ohlc)
  , ticker_string_(ticker)
{
  ui->setupUi(this);
  ui->ticker->setText(ticker.data());
  //
  // Create candlestick plot
  //
  crypto_price_plot_ = new ohlc_price_plot(this, hdf5_ohlc_);
  ui->candlestick_layout->addWidget(crypto_price_plot_);

  //
  // Create stream/filters plot
  //
  assets_plot_ = new indicator_plot(this);
  ui->indicators_layout->addWidget(assets_plot_);
  assets_plot_->setMinimumHeight(128);
  assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

  filters_plot_ = new indicator_plot(this);
  ui->indicators_layout->addWidget(filters_plot_);
  filters_plot_->setMinimumHeight(128);
  filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

  QStringList slist("Auto");
  for (const auto& r : ohlc_data_resolutions::available_resolutions())
  {
    slist << r.name_;
  }
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

  QHeaderView* verticalHeader = ind_vis_->verticalHeader();
  verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
  verticalHeader->setDefaultSectionSize(24);
  verticalHeader->setVisible(false);
  ui->controls_layout->addWidget(ind_vis_);

  btn_indicator_ = new QPushButton(this);
  btn_indicator_->setText("Indicators");
  btn_indicator_->setFlat(true);
  ui->controls_layout->addWidget(btn_indicator_);

  DigitalClock* clock = new DigitalClock(this, global_settings()->get_global_clock_timer());
  ui->controls_layout->addWidget(clock);
  //
  connect_gui();
}

// ----------------------------------------------------------------------------
price_chart_widget::~price_chart_widget()
{
  delete ui;
  delete crypto_price_plot_;
  delete filters_plot_;
  delete assets_plot_;
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

  connect(
    ui->candle_res, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
    [this](int index) {
      if (index > 0)
      {
        double res = ohlc_data_resolutions::available_resolutions()[index - 1];
        crypto_price_plot_->set_auto_candle_resolution(false);
        if (crypto_price_plot_->adjust_candle_size(res))
        {
          crypto_price_plot_->adjust_data_scaling();
        }
        crypto_price_plot_->replot();
      }
      else
      {
        crypto_price_plot_->set_auto_candle_resolution(true);
        if (crypto_price_plot_->adjust_candle_size(0))
        {
          crypto_price_plot_->adjust_data_scaling();
        }
        crypto_price_plot_->replot();
      }
    },
    Qt::QueuedConnection);

  connect(
    ui->heikin, QOverload<int>::of(&QCheckBox::stateChanged), this,
    [this](int state) {
      if (state)
      {
        crypto_price_plot_->setMode(ohlc_chart_curve::HeikinAshi);
      }
      else
      {
        crypto_price_plot_->setMode(QwtPlotTradingCurve::SymbolStyle::CandleStick);
      }
    },
    Qt::QueuedConnection);

  connect(
    crypto_price_plot_->get_interactor(), &ohlc_interactor::repair_pressed, this,
    [this](QPointF p) {
      auto crosshairs = crypto_price_plot_->get_crosshairs();
      double msecs = crosshairs->quantize_x_coord(p.x());
      const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msecs);
      QString s = QLocale::system().toString(dt, "dd-MM-yy hh:mm");

      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(
        this, "Confirm", "Delete from " + s, QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes)
      {
        hdf5_ohlc_->truncate_from_time(msecs);
        pplot_dbg<0>.error(str<>("emit update_candlestick_data"));
        // update_candlestick_data();
      }
      else
      {
        pplot_dbg<0>.debug(str<>("Yes *not* clicked"));
      }
    },
    Qt::QueuedConnection);

  connect(
    crypto_price_plot_, &ohlc_price_plot::plotScaleChanged, this,
    [this](double t1, double t2) {
      filters_plot_->update_time_axis(t1, t2);
      assets_plot_->update_time_axis(t1, t2);

      // axisScaleDraw(QwtPlot::xBottom)->, crypto_price_plot_->axisScaleDraw(QwtPlot::xBottom));
      // filters_plot_->setAxisScaleEngine(QwtPlot::xBottom, crypto_price_plot_->axisScaleEngine(QwtPlot::xBottom));
    },
    Qt::QueuedConnection);

  connect(ind_vis_, &QTableView::clicked, this, [this](const QModelIndex& i) {
    int col = i.column();
    int row = i.row();
    if (col == 2)
    {
      auto it = std::next(ind_model_.indicators_.begin(), row);
      QColor c = it->curve->pen().color();
      QColor color = QColorDialog::getColor(c, this);
      if (color.isValid())
      {
        QPen new_pen(it->curve->pen());
        new_pen.setColor(color);
        it->curve->setPen(new_pen);
      }
      ind_model_.dataAdded();
      this->replot();
    }
    else if (col == 3)
    {
      auto it = std::next(ind_model_.indicators_.begin(), row);
      it->curve->detach();
      delete it->curve;
      ind_model_.indicators_.erase(it);
      ind_model_.dataAdded();
      this->replot();
    }
  });

  connect(btn_indicator_, &QPushButton::clicked, this, [this](bool b) {
    pplot_dbg<0>.debug(str<>("Indicators"), exchange_->name(), ticker_string_);

    indicator_dialog in_dialog = indicator_dialog();
    auto result = in_dialog.exec();
    if (result == QDialog::Accepted)
    {
      static int colour_count = 0;
      // copy the algorithm out of the dialog
      auto indicator = in_dialog.get_algorithm();
      // now execute the algorithm
      std::visit(
        [this](auto& alg) {
          // first - initialize algorithm with parameters (set by dialog)
          alg.initialize();

          // convert the dataset name selections in the dialog into actual datasets
          std::vector<ohlc_datasets*> datasets = indicators::get_datasets(alg.params, hdf5_ohlc_);

          QVector<QPointF> indicator_plot;
          // if the algorithm operates on a single input dataset
          if (datasets.size() == 1)
          {
            const auto& input_dataset = datasets[0]->ohlc_samples_;
            indicator_plot.reserve(input_dataset->size());

            // iterate over the dataset, executing the algorithm for each point
            for (auto const& ohlc : input_dataset->data())
            {
              double val = alg.operator()(ohlc);
              QPointF xyval(ohlc.time, val);
              indicator_plot.push_back(xyval);
            }
          }
          else
          {
            pplot_dbg<0>.error(str<>("Indicator"), alg.name, "Not yet implemented");
          }

          QColor colours[10] = {QColor("cyan"), QColor("magenta"), QColor("red"), QColor("darkRed"),
            QColor("darkCyan"), QColor("darkMagenta"), QColor("green"), QColor("darkGreen"),
            QColor("yellow"), QColor("blue")};
          auto colour = colours[colour_count++ % 10];

          QString name = QString(alg.name.c_str());
          QwtPlotCurve* curve;
          if (alg.price_overlay)
          {
            curve = crypto_price_plot_->add_overlay_curve(name, indicator_plot, colour);
          }
          else
          {
            curve = add_indicator_plot(name, indicator_plot, colour);
          }

          QString params = QString(indicators::param_string(alg.params).c_str());
          ind_model_.indicators_.push_back({name, params, curve});
          ind_model_.dataAdded();
          //          QStandardItem* item = new QStandardItem();
          //          item->setText(name);
          //          item->setCheckable(true);
          //          item->setCheckState(Qt::Checked);
          //          item->setIcon(QCommonStyle().standardIcon(QStyle::SP_TrashIcon));
          //          ind_model_.appendRow(item);
          this->replot();
        },
        indicator);

      //indicators::generate(indicator, hdf5_ohlc_->)

      //            price_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);

      //            filters_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
      //            filters_plot_->setAxisScale(QwtAxis::YRight, 0, 1);

      //            assets_plot_->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
      //            assets_plot_->setAxisScale(QwtAxis::YRight, 0, 1);
    }

    //    indicators::moving_average ma{};
    //    ma.generate(hdf5_ohlc_);
  });
  //    connect(pAction2, SIGNAL(triggered()), this, SLOT(onAction2()));
  //    connect(pAction3, SIGNAL(triggered()), this, SLOT(onAction3()));

  filters_plot_->hide();
  assets_plot_->hide();
}

// ----------------------------------------------------------------------------
// slot to ensure widget updates on GUI thread
void price_chart_widget::graph_rescale(int range)
{
  auto last_time = hdf5_ohlc_->get_last_sample_time(true);
  double t1 = 0, t2 = last_time;
  if (range == -2)
  {
    t1 = last_time - 0.25 * ohlc_data_resolutions::day;
  }
  else if (range == -1)
  {
    t1 = last_time - 0.5 * ohlc_data_resolutions::day;
  }
  else if (range == 0)
  {
    t1 = last_time - 1.0 * ohlc_data_resolutions::day;
  }
  else if (range == 1)
  {
    t1 = last_time - 7 * ohlc_data_resolutions::day;
  }
  else if (range == 2)
  {
    t1 = last_time - 31 * ohlc_data_resolutions::day;
  }
  else if (range == 3)
  {
    t1 = last_time - 365 * ohlc_data_resolutions::day;
  }
  // special case, to extend current view with new data
  else if (range == 100)
  {
    t1 = last_time - 365 * ohlc_data_resolutions::day;
  }
  else
  {
    t1 = hdf5_ohlc_->get_first_sample_time();
  }
  crypto_price_plot_->update_time_axis(t1, t2);
}

// ----------------------------------------------------------------------------
void price_chart_widget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  bool changed = crypto_price_plot_->update_candle_size();
  pplot_dbg<5>.debug(str<>("Resize"), "res changed", changed);
}

// ----------------------------------------------------------------------------
void price_chart_widget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  bool changed = crypto_price_plot_->update_candle_size();
  pplot_dbg<5>.debug(str<>("Show"), "res changed", changed);
}

// ----------------------------------------------------------------------------
indicators_model::indicators_model(QObject* parent)
  : QAbstractTableModel(parent)
{
}

int indicators_model::rowCount(const QModelIndex& /*parent*/) const
{
  return indicators_.size();
}

int indicators_model::columnCount(const QModelIndex& /*parent*/) const
{
  return 4;
}

QVariant indicators_model::data(const QModelIndex& index, int role) const
{
  QVariant result;
  if (!index.isValid())
  {
    return result;
  }

  auto it = std::next(indicators_.begin(), index.row());
  if (role == Qt::DisplayRole)
  {
    if (index.column() == 0)
    {
      return (QString(it->text));
    }
    else if (index.column() == 1)
    {
      return (QString(it->params));
    }
    else if (index.column() == 2)
    {
      return (QString(""));
    }
  }
  else if (role == Qt::BackgroundRole && index.column() == 2)
  {
    QColor col = it->curve->pen().color();
    return col;
  }
  else if (role == Qt::DecorationRole && index.column() == 3)
  {
    return (QCommonStyle().standardIcon(QStyle::SP_TrashIcon));
  }

  return result;
}

void indicators_model::dataAdded()
{
  beginResetModel();
  QModelIndex topLeft = createIndex(0, 0);
  QModelIndex bottomRight = createIndex(indicators_.size(), 2);
  emit QAbstractTableModel::dataChanged(topLeft, bottomRight);
  endResetModel();
}

// ----------------------------------------------------------------------------
QwtPlotCurve* price_chart_widget::add_indicator_plot(
  const QString& title, const QVector<QPointF>& samples, const QColor& color)
{
  auto m_curve = new QwtPlotCurve(title);
  m_curve->setYAxis(QwtPlot::yRight);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::Lines);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
  m_curve->setPen(color, 2);
  m_curve->setSamples(samples);
  m_curve->attach(filters_plot_);

  // Align the right axis of the indicator with the main price plot
  auto* scaleWidget = crypto_price_plot_->axisWidget(QwtPlot::yRight);
  double extent = scaleWidget->scaleDraw()->extent(scaleWidget->font());
  filters_plot_->axisWidget(QwtPlot::yRight)->scaleDraw()->setMinimumExtent(extent);
  //filters_plot_->updateLayout();
  //filters_plot_->replot();
  filters_plot_->show();
  return m_curve;
}
