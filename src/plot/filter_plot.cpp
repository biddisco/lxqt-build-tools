// STL
#include <cassert>
#include <iostream>
#include <sstream>
// Qt
#include <QDateTime>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QWheelEvent>
// Qwt
#include <QwtDateScaleDraw>
#include <QwtDateScaleEngine>
#include <QwtPlot>
#include <QwtPlotCurve>
#include <QwtPlotDirectPainter>
#include <QwtPlotGrid>
#include <QwtPlotLayout>
#include <QwtPlotLegendItem>
#include <QwtPlotRenderer>
#include <QwtPlotTextLabel>
#include <QwtScaleMap>
#include <QwtScaleWidget>
#include <QwtSeriesData>
#include <QwtSymbol>
#include <QwtTextLabel>
// Grox
#include "debug/print.hpp"
#include "plot/filter_plot.hpp"
#include "plot/ohlc_chart_curve.hpp"
#include "plot/ohlc_chart_data.hpp"
#include "plot/ohlc_date_scaledraw.hpp"
#include "plot/ohlc_interactor.hpp"
#include "plot/ohlc_picker.hpp"
//
#include <range/v3/view.hpp>

// ----------------------------------------------------------------------------
filter_plot::filter_plot(QWidget* parent)
  : QwtPlot(parent)
  , timescaleDraw_(nullptr)
  , timescaleEngine_(nullptr)
{
  QwtText text(" ");
  text.setColor(Qt::lightGray);
  setTitle(text);

  // find difference between local time and UTC, for 'correct' date/time axis
  QDateTime local(QDateTime::currentDateTime());
  QDateTime UTC(local.toUTC());
  QDateTime dt(UTC.date(), UTC.time(), Qt::LocalTime);

  // X axis : setup date/time axis scaling and tick draw
  timescaleDraw_ = new ohlc_date_scaledraw(Qt::TimeSpec::OffsetFromUTC);
  timescaleEngine_ = new QwtDateScaleEngine(Qt::TimeSpec::OffsetFromUTC);
  timescaleDraw_->setUtcOffset(dt.secsTo(local));
  timescaleEngine_->setUtcOffset(dt.secsTo(local));

  // Adjust the RHS of the X axis. Otherwise, there is space on the RHS.
  timescaleEngine_->setAttribute(QwtScaleEngine::Floating, true);
  setAxisScaleDraw(QwtPlot::xBottom, timescaleDraw_);
  setAxisScaleEngine(QwtPlot::xBottom, timescaleEngine_);
  setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignCenter | Qt::AlignBottom);

  // No auto scaling - we do scaling in the interactor zoom/pan class
  setAxisAutoScale(QwtPlot::yLeft, false);
  setAxisAutoScale(QwtPlot::yRight, false);
  setAxisAutoScale(QwtPlot::xBottom, false);
  //
  setAxisVisible(QwtAxis::YLeft, false);
  setAxisVisible(QwtAxis::YRight, true);

  // QWidget : fill background before painting (color = QPalette::Window)
  setAutoFillBackground(true);

  // main canvas color - dark, but not black
  static const QColor c("#18191b");

  // palette for widget colours
  QPalette palette0 = palette();
  palette0.setColor(QPalette::Window, c);
  canvas()->setPalette(palette0);
  setPalette(palette0);

  // x axis colours
  QPalette palette1 = axisWidget(Axis::xBottom)->palette();
  palette1.setColor(QPalette::WindowText, Qt::lightGray);    // ticks
  palette1.setColor(QPalette::Text, Qt::lightGray);          // tick labels
  axisWidget(Axis::xBottom)->setPalette(palette1);

  // y axis colours
  QPalette palette2 = axisWidget(Axis::yRight)->palette();
  palette2.setColor(QPalette::WindowText, Qt::lightGray);    // ticks
  palette2.setColor(QPalette::Text, Qt::lightGray);          // tick labels
  axisWidget(Axis::yRight)->setPalette(palette2);

  // Attach a dotted-line grid to the plot
  QwtPlotGrid* grid = new QwtPlotGrid();
  grid->setYAxis(QwtPlot::yRight);
  grid->setItemAttribute(grid->Legend, false);
  grid->setPen(QColor(Qt::darkGray), 0.0, Qt::PenStyle::DotLine);
  grid->attach(this);

  // Override the Qt size policy. Otherwise, the plot may not scale to
  // the desired dimensions from the grid layout.
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setMinimumSize(0, 0);
}

// ----------------------------------------------------------------------------
filter_plot::~filter_plot() {}

// ----------------------------------------------------------------------------
void filter_plot::showItem(QwtPlotItem* item, bool on)
{
  item->setVisible(on);
  replot();
}

// ----------------------------------------------------------------------------
void filter_plot::update_time_axis(double t1, double t2)
{
  const bool doAutoReplot = autoReplot();
  setAutoReplot(false);

  // update the X axis with new min max
  setAxisScale(QwtAxis::XBottom, t1, t2);
  /*
    // find the min/max price for this new range
    auto minmax = ohlc_dataset_view_->get_min_max_window(
                get_candle_resolution(), t1, t2, 0.05);

    // update the Y price axis with min max
    setAxisScale(QwtAxis::YRight, minmax.min_price_, minmax.max_price_);

    // update the Y volume axis with min max
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
*/
  setAutoReplot(doAutoReplot);
  replot();
}

// ----------------------------------------------------------------------------
void filter_plot::add_asset_curve(
  const QString& title, const QVector<QPointF>& samples, const QColor& color)
{
  auto m_curve = new QwtPlotCurve(title);
  m_curve->setYAxis(QwtPlot::yRight);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::NoCurve);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);

  QwtSymbol* symbol = new QwtSymbol(QwtSymbol::XCross);
  symbol->setSize(4);
  symbol->setPen(color, 1);
  m_curve->setSymbol(symbol);

  m_curve->setSamples(samples);
  m_curve->attach(this);

  double ymin = axisScaleDiv(QwtAxis::YRight).lowerBound();
  double ymax = axisScaleDiv(QwtAxis::YRight).upperBound();
  ymax = std::max(ymax, m_curve->maxYValue());
  ymin = std::min(ymin, m_curve->minYValue());

  setAxisScale(QwtAxis::YRight, ymin, ymax);
}
