// STL
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
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
#include "data/ohlc_utils.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"
#include "plot/ohlc_chart_curve.hpp"
#include "plot/ohlc_date_scaledraw.hpp"
#include "plot/ohlc_interactor.hpp"
#include "plot/ohlc_picker.hpp"
#include "plot/ohlc_price_plot.hpp"
#include "plot/timebased_data_curve.hpp"
#include "util/datetime_utils.hpp"
//
#include <range/v3/view.hpp>
#include <fmt/format.h>

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 0;
//
template <int Level>
inline constexpr print_threshold<Level, debug_level> plot_dbg("OHLCplot");

// ----------------------------------------------------------------------------
void fill_text_label(QwtText& label)
{
  QColor cc("#ffffff");
  cc.setAlpha(100);
  label.setBorderPen(QPen(cc, 2));
  cc.setAlpha(50);
  label.setBackgroundBrush(cc);
}

// ----------------------------------------------------------------------------
// Just a simple override to make the number of decimals consistent
class ohlc_price_scaledraw : public QwtScaleDraw
{
  int dig_;
  int dec_;
  char form_;
  std::string fstr;

  public:
  // pass in max-min of estimated scale range to best fit digits etc
  ohlc_price_scaledraw(double range)
    : QwtScaleDraw()
  {
    form_ = 'f';
    dec_ = 4;
    int exponent = 0;
    if (range < 0) { throw std::logic_error("invalid range in graph axes"); }
    else { exponent = static_cast<int>(std::floor(std::log10(range))); }

    // if negative we need decimal places
    if (exponent < 0)
    {
      dec_ = 2 - exponent;
      dig_ = dec_ + 2;
      if (dec_ > 4)
      {
        form_ = 'e';
        dec_ = 3;
        dig_ = dec_ + 6;
      }
    }
    else if (exponent > 3)
    {
      dec_ = 0;
      form_ = 'f';
      dig_ = exponent + 1;
    }
    else { dig_ = dec_ + 2; }
    fstr = fmt::format("%{}.{}{}", dig_, dec_, form_);
    plot_dbg<0>.debug(str<>("Format string"), range, exponent, dig_, dec_, form_, fstr);
  }

  QwtText label(double value) const QWT_OVERRIDE
  {
    return QwtText(QString().asprintf(fstr.c_str(), value), QwtText::TextFormat::PlainText);
  }
};

// ----------------------------------------------------------------------------
ohlc_price_plot::ohlc_price_plot(QWidget* parent, std::shared_ptr<ohlc_dataset_view> data)
  : timebased_chart_plot(parent)
  , direct_painter_(nullptr)
  , ohlc_dataset_view_(data)
  , auto_candle_resolution_(true)
  , first_update_(true)
  , last_auto_res_(-1)
{
  // find a fixed font char size for candle status/data
  QString X = "X";
  QFont label_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixed_char_size_x_ = QFontMetrics(label_font).tightBoundingRect(X).width();
  fixed_char_size_y_ = QFontMetrics(label_font).tightBoundingRect(X).height();
  int const margin = 0.5 * fixed_char_size_y_;       // margin space in x and y
  int const indent = 4;                              // text offset in x direction
  int const label_xtext = 3 * fixed_char_size_x_;    // "15d", "30m" etc
  int const label_xsize = 2 * indent + 2 * margin + label_xtext;

  // setup small label we use to show current candle resolution
  candle_label_ = new QwtTextLabel(this);
  candle_label_->setIndent(indent);
  candle_label_->setMargin(margin);
  candle_label_->setFont(label_font);
  candle_label_->setGeometry(0, 0, label_xsize, 2 * margin + 2 * fixed_char_size_y_);

  // setup label that shows candle stats as crosshairs move around
  candle_status_ = new QwtTextLabel(this);
  candle_status_->setIndent(indent);
  candle_status_->setMargin(margin);
  candle_status_->setFont(label_font);
  // OHLCV format string : "O:<num>" = 5(OHLCV)*2 + 4(OHLC)*9 + 1(V)*14 = 61chars
  candle_status_->setGeometry(
      label_xsize, 0, 65 * fixed_char_size_x_, 2 * margin + 2 * fixed_char_size_y_);

  // default start up resolution
  candle_resolution_ = ohlc_data_resolutions::minute;

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

  ohlcv_minmax minmax = data->get_min_max(ohlc_data_resolutions::minute,
      data->get_first_sample_time(), data->get_last_sample_time_msec(false));

  // Y axis : setup price axis scaling and tick draw
  // NB : We do not need to explicitly set a left Y axis (volume)
  // the default axis can be used even when not visible
  pricescaleDraw_ = new ohlc_price_scaledraw(minmax.max_price_ - minmax.min_price_);
  setAxisScaleDraw(QwtPlot::yRight, pricescaleDraw_);

  // No auto scaling - we do scaling in the interactor zoom/pan class
  setAxisAutoScale(QwtPlot::yLeft, false);
  setAxisAutoScale(QwtPlot::yRight, false);
  setAxisAutoScale(QwtPlot::xBottom, false);
  // right axis shows price, left volume axis is hidden
  setAxisVisible(QwtAxis::YLeft, false);
  setAxisVisible(QwtAxis::YRight, true);

  // bring the graph slightly inside the borders to leave a small outer margin
  setContentsMargins(2, 2, 2, 2);

  // A custom interactor for zooming/panning
  plot_interactor_ = new ohlc_interactor(this);

  // Custom crosshairs to show current cursor pos
  timebased_chart_plot::crosshairs_ = new ohlc_picker(canvas());

  // Attach a dotted-line grid to the plot
  QwtPlotGrid* grid = new QwtPlotGrid();
  grid->setYAxis(QwtPlot::yRight);
  grid->setItemAttribute(grid->Legend, false);
  grid->setPen(QColor(Qt::darkGray), 0.0, Qt::PenStyle::DotLine);
  grid->attach(this);

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

  // yr axis colours
  QPalette palette2 = axisWidget(Axis::yRight)->palette();
  palette2.setColor(QPalette::WindowText, Qt::lightGray);    // ticks
  palette2.setColor(QPalette::Text, Qt::lightGray);          // tick labels
  axisWidget(Axis::yRight)->setPalette(palette2);

  // Override the Qt size policy. Otherwise, the plot may not scale to
  // the desired dimensions from the grid layout.
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumSize(128, 64);

  bind_graphs();
}

// ----------------------------------------------------------------------------
ohlc_price_plot::~ohlc_price_plot() {}

// ----------------------------------------------------------------------------
void ohlc_price_plot::bind_graphs()
{
  auto resolutions = ohlc_dataset_view_->get_dataset_resolutions();
  bool first = true;
  for (auto r : resolutions)
  {
    auto* data = ohlc_dataset_view_->get_dataset(r);
    auto* curve = new ohlc_chart_curve(data);
    curves_.insert(std::make_pair(r, curve));

    if (r == ohlc_data_resolutions::minute)
    {
      auto* live_data = ohlc_dataset_view_->get_live_data(ohlc_data_resolutions::minute);
      auto* live_curve = new ohlc_chart_curve(live_data);
      live_curves_.insert(std::make_pair(r, live_curve));
      live_curve->setSymbolPen(QwtPlotTradingCurve::Increasing, QColor("#26a69a"));
      live_curve->setSymbolPen(QwtPlotTradingCurve::Decreasing, QColor("#FFBF00"));
      live_curve->setSymbolBrush(QwtPlotTradingCurve::Increasing, QColor("#26a69a"));
      live_curve->setSymbolBrush(QwtPlotTradingCurve::Decreasing, QColor("#FFBF00"));
    }

    // bind it to this plot
    curve->attach(this);
    curve->setVisible(first);
    first = false;
  }
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_live_data(ohlctv_sample const& new_sample)
{
  ohlc_chart_curve* live_curve;
  // repaint the live dataset
  if (!direct_painter_)
  {
    direct_painter_ = new QwtPlotDirectPainter(this);
    // qwt directpainter used inside a Qt-AdvancedDockingSystem window
    // has bad repaint effects unless we turn on CopyBackingStore
    direct_painter_->setAttribute(QwtPlotDirectPainter::FullRepaint, false);
    direct_painter_->setAttribute(QwtPlotDirectPainter::CopyBackingStore, true);
    live_curve = live_curves_[ohlc_data_resolutions::minute];
    live_curve->attach(this);
    live_curve->setVisible(true);
  }
  else
  {
    live_curve = live_curves_[ohlc_data_resolutions::minute];
    live_curve->itemChanged();
  }
  direct_painter_->drawSeries(live_curve, 0, live_curve->data()->size() - 1);
}

// ----------------------------------------------------------------------------
bool ohlc_price_plot::adjust_candle_size(double res)
{
  // auto mode
  if (res == 0)
  {
    // if we don't find a usable coarser resolution, use 1m
    res = ohlc_data_resolutions::minute;
    // get the pixel/plot coordinate transform
    const QwtScaleMap map = canvasMap(QwtAxis::XBottom);
    // try for candle around ~10 pixels - How big in world coords?
    double xm = map.invTransform(10) - map.invTransform(0);
    for (auto const& r : ranges::views::reverse(ohlc_data_resolutions::available_resolutions()))
    {
      if (r < xm)
      {
        // if we have not changed value, just exit
        if (r == last_auto_res_) return false;
        res = last_auto_res_ = r;
        //
        break;
      }
    }
  }
  else { last_auto_res_ = 0; }
  // user selected resolution
  for (auto const& r : ohlc_data_resolutions::available_resolutions())
  {
    auto* data = ohlc_dataset_view_->get_dataset(r);
    if (!curves_.contains(r)) continue;

    auto* curve = curves_[r];
    curve->setVisible(r == res);
    if (r == res)
    {
      curves_[r]->setSymbolExtent(0.8 * r);
      QwtText candle_label(r.name_);
      candle_label.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
      if (auto_candle_resolution())
        candle_label.setColor(Qt::magenta);
      else
        candle_label.setColor(Qt::red);
      //fill_text_label(candle_label);
      candle_label_->setText(candle_label);
    }
  }
  candle_resolution_ = res;
  return true;
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::setMode(int style)
{
  QwtPlotTradingCurve::SymbolStyle symbolStyle =
      static_cast<QwtPlotTradingCurve::SymbolStyle>(style);

  QwtPlotItemList curves = itemList(QwtPlotItem::Rtti_PlotTradingCurve);
  for (int i = 0; i < curves.size(); i++)
  {
    QwtPlotTradingCurve* curve = static_cast<QwtPlotTradingCurve*>(curves[i]);
    curve->setSymbolStyle(symbolStyle);
  }

  replot();
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::showItem(QwtPlotItem* item, bool on)
{
  item->setVisible(on);
  replot();
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::exportPlot()
{
  QwtPlotRenderer renderer;
  renderer.exportTo(this, "grox.pdf");
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_time_axis(double t1, double t2, bool emit_signal)
{
  bool const doAutoReplot = autoReplot();
  setAutoReplot(false);

  // update the X axis with new min max
  setAxisScale(QwtAxis::XBottom, t1, t2);

  // recompute the mapping to/from world/pixels
  updateAxes();

  // we need the min/max price for the new time range
  ohlcv_minmax minmax;
  bool candles_changed = false;

  // if the mapping has changed a lot, we might need to change candle sizes
  // scale change might trigger a candle resolution update
  if (auto_candle_resolution()) { adjust_candle_size(0); }
  minmax = ohlc_dataset_view_->get_min_max_window(get_candle_resolution(), t1, t2, 0.05);

  // update the Y price axis with min max
  setAxisScale(QwtAxis::YRight, minmax.min_price_, minmax.max_price_);

  // update the Y volume axis with min max (candle res changes might change
  // volume bars as they sum more/less data)
  setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);

  plot_dbg<8>.debug(str<>("min_max"),
      ohlc_data_resolutions::get_resolution(get_candle_resolution()).name_,
      msecs_unix_to_calendar_time(t1), "->", msecs_unix_to_calendar_time(t2), "(",
      minmax.min_price_, ",", minmax.max_price_, ")");

  setAutoReplot(doAutoReplot);
  replot();
  if (emit_signal) emit timeAxisChanged(t1, t2, false);
}

// ----------------------------------------------------------------------------
double ohlc_price_plot::quantize_x_coord(double x)
{
  double res = get_candle_resolution();
  double p1 = res * static_cast<uint64_t>((x + res / 2.0) / res);
  return p1;
}

// ----------------------------------------------------------------------------
bool ohlc_price_plot::update_candle_size()
{
  bool changed = false;
  if (auto_candle_resolution()) { changed = adjust_candle_size(0); }
  else { changed = adjust_candle_size(get_candle_resolution()); }
  if (changed) { adjust_data_scaling(); }
  return changed;
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::adjust_data_scaling()
{
  double const t1 = axisScaleDiv(QwtAxis::XBottom).lowerBound();
  double const t2 = axisScaleDiv(QwtAxis::XBottom).upperBound();
  if (t2 > t1)
  {
    auto minmax = ohlc_dataset_view_->get_min_max_window(get_candle_resolution(), t1, t2, 0.05);
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
  }
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::display_picker_info(const QPointF pos)
{
  double const time = pos.x();
  int64_t index = -1;
  auto* dataset = ohlc_dataset_view_->get_dataset(get_candle_resolution());
  if (dataset->size() > 0) { index = dataset->sample_index(time); }
  if (index < 0 || size_t(index) >= dataset->size())
  {
    candle_status_->setText("");
    return;
  }
  //
  ohlctv_sample const& sample = dataset->data().at(index);

  char const* c = "red";
  if (sample.open <= sample.close) c = "green";

  std::string fstr = fmt::format(                                  //
      "<font color=\"white\"> O: <font color=\"{}\">{:<9.5f}"      //
      "<font color=\"white\"> H: <font color=\"{}\">{:<9.5f}"      //
      "<font color=\"white\"> L: <font color=\"{}\">{:<9.5f}"      //
      "<font color=\"white\"> C: <font color=\"{}\">{:<9.5f}"      //
      "<font color=\"white\"> V: <font color=\"{}\">{:<14.2f}",    //
      c, sample.open,                                              //
      c, sample.high,                                              //
      c, sample.low,                                               //
      c, sample.close,                                             //
      c, sample.volume                                             //
  );

  QwtText status(fstr.c_str());
  status.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
  //fill_text_label(status);
  candle_status_->setText(status);
}

// ----------------------------------------------------------------------------
QwtPlotCurve* ohlc_price_plot::add_buy_sell_curve(
    QString const& title, QVector<QPointF> const& samples, QColor const& color)
{
  auto m_curve = new QwtPlotCurve(title);
  m_curve->setYAxis(QwtPlot::yRight);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::NoCurve);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);

  QwtSymbol* symbol = new QwtSymbol(QwtSymbol::XCross);
  symbol->setSize(16);
  symbol->setPen(color, 4);
  m_curve->setSymbol(symbol);

  m_curve->setSamples(samples);
  m_curve->attach(this);
  return m_curve;
}

// ----------------------------------------------------------------------------
timebased_data_curve* ohlc_price_plot::add_overlay_curve(
    QString const& title, point_chart_data* data, QColor const& color)
{
  auto m_curve = new timebased_data_curve(title);
  m_curve->setYAxis(QwtPlot::yRight);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::Lines);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
  m_curve->setPen(color, 2);

  //    QwtSymbol* symbol = new QwtSymbol(QwtSymbol::XCross);
  //    symbol->setSize(16);
  //    symbol->setPen(color, 4);
  //    m_curve->setSymbol(symbol);

  m_curve->setData(data);
  m_curve->attach(this);
  return m_curve;
}

// ----------------------------------------------------------------------------
timebased_data_curve* ohlc_price_plot::add_overlay_volume_curve(
    QString const& title, point_chart_data* data, QColor const& color)
{
  auto m_curve = new timebased_data_curve(title);
  m_curve->setYAxis(QwtPlot::yLeft);
  m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
  m_curve->setStyle(QwtPlotCurve::Lines);
  m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
  m_curve->setPen(color, 2);
  m_curve->setData(data);
  m_curve->attach(this);
  return m_curve;
}
// ----------------------------------------------------------------------------
void ohlc_price_plot::updateLayout()
{
  QwtPlot::updateLayout();
  if (first_update_)
  {
    plot_dbg<5>.debug(str<>("updateLayout"), "adjust_candle_size");
    update_candle_size();
    first_update_ = false;
  }
}
