// STL
#include <iostream>
// Qt
#include <QDateTime>
#include <QFont>
#include <QPalette>
// Qwt
#include <QwtLinearScaleEngine>
#include <QwtPlot>
#include <QwtPlotGrid>
#include <QwtPlotLegendItem>
#include <QwtPlotRenderer>
#include <QwtScaleWidget>
#include <QwtText>
// Grox
#include "debug/print.hpp"
#include "plot/OrderBookCurve.h"
#include "plot/OrderBookPlot.h"

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 9;
//
template <int Level>
static print_threshold<Level, debug_level> book_dbg("ord-plot");

// ----------------------------------------------------------------------------
OrderBookPlot::OrderBookPlot(QWidget* parent, std::shared_ptr<order_book_base> order_book)
  : QwtPlot(parent)
  , order_book_(order_book)
{
  //setTitle("a title");

  QwtLinearScaleEngine* scaleEngine = new QwtLinearScaleEngine(10);

  setAxisScaleEngine(QwtPlot::xBottom, scaleEngine);
  setAxisLabelRotation(QwtPlot::xBottom, 0.0);
  setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignLeft | Qt::AlignBottom);

  // The following is needed to properly adjust the RHS of the X axis. Otherwise,
  // there is space on the RHS.
  axisScaleEngine(QwtPlot::xBottom)->setAttribute(QwtScaleEngine::Floating, true);

  // LeftButton for the zooming
  // MidButton for the panning
  // RightButton: zoom out by 1
  // Ctrl+RighButton: zoom out to full size

  //    QwtPlotMagnifier *zoom_x = new QwtPlotMagnifier( canvas() );
  //    zoom_x->setWheelModifiers(Qt::ShiftModifier);
  //    zoom_x->setAxisEnabled(Qt::XAxis, false);
  //    zoom_x->setAxisEnabled(Qt::YAxis, true);
  //    zoom_x->setAxisEnabled(Qt::ZAxis, false);

  //    QwtPlotMagnifier *zoom_y = new QwtPlotMagnifier( canvas() );
  //    zoom_y->setWheelModifiers(Qt::ControlModifier);
  //    zoom_y->setAxisEnabled(Qt::XAxis, true);
  //    zoom_y->setAxisEnabled(Qt::YAxis, false);
  //    zoom_y->setAxisEnabled(Qt::ZAxis, false);

  // Attach a dotted-line grid to the plot.
  QwtPlotGrid* grid = new QwtPlotGrid();
  grid->setItemAttribute(grid->Legend, false);
  grid->setPen(QColor(Qt::lightGray), 0.0, Qt::PenStyle::DotLine);
  grid->attach(this);

  // Override the size policy. Otherwise, the plot may not scale to
  // the desired dimensions from the grid layout.
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setMinimumSize(0, 0);

  // Attach a legend internal to the plot
  QwtPlotLegendItem* legend = new QwtPlotLegendItem();
  legend->setAlignmentInCanvas(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  legend->attach(this);

  // main canvas color - dark, but not black
  static const QColor c(0x28, 0x28, 0x28);

  // QWidget : fill background before painting (color = QPalette::Window)
  setAutoFillBackground(true);

  // palette for widget
  QPalette palette0 = palette();
  palette0.setColor(QPalette::Window, c);
  canvas()->setPalette(palette0);
  setPalette(palette0);

  // Font for Axis titles
  axis_title_font = QFont("Times", 10, QFont::Bold);

  // x axis
  QPalette palette1 = axisWidget(Axis::xBottom)->palette();
  palette1.setColor(QPalette::WindowText, Qt::lightGray);    // ticks
  palette1.setColor(QPalette::Text, Qt::lightGray);          // tick labels
  axisWidget(Axis::xBottom)->setPalette(palette1);

  QwtText axisTitleX("Price");
  axisTitleX.setRenderFlags(Qt::AlignRight | Qt::AlignVCenter);
  axisTitleX.setFont(axis_title_font);
  this->setAxisTitle(QwtPlot::xBottom, axisTitleX);

  // y axis
  QPalette palette2 = axisWidget(Axis::yLeft)->palette();
  palette2.setColor(QPalette::WindowText, Qt::green);    // ticks
  palette2.setColor(QPalette::Text, Qt::red);            // tick labels
  axisWidget(Axis::yLeft)->setPalette(palette2);

  QwtText axisTitleY("Bitstamp");
  axisTitleY.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
  axisTitleY.setFont(axis_title_font);
  this->setAxisTitle(QwtPlot::yLeft, axisTitleY);

  // 2nd y axis
  // QPalette palette3 = axisWidget(Axis::yRight)->palette();
  // palette3.setColor(QPalette::WindowText, Qt::darkYellow);    // tick
  // palette3.setColor(QPalette::Text, Qt::darkMagenta);         // tick labels
  // axisWidget(Axis::yRight)->setPalette(palette3);

  // QwtText axisTitleY2("XRPL");
  // axisTitleY2.setRenderFlags(Qt::AlignRight | Qt::AlignTop);
  // axisTitleY2.setFont(axis_title_font);
  // this->setAxisTitle(QwtPlot::yRight, axisTitleY2);

  // enableAxis(QwtPlot::yRight);

  bid_curve_ = new OrderBookCurve();
  ask_curve_ = new OrderBookCurve();
  //
  // if (secondaxis)
  // {
  //   bid_curve_->setSegmentInfo(0, 0, Qt::darkYellow, 3);
  //   ask_curve_->setSegmentInfo(0, 0, Qt::darkMagenta, 3);
  //   bid_curve_->setYAxis(QwtPlot::yRight);
  //   ask_curve_->setYAxis(QwtPlot::yRight);
  // }
  // else
  {
    bid_curve_->setSegmentInfo(0, 0, Qt::green, 3);
    ask_curve_->setSegmentInfo(0, 0, Qt::red, 3);
    bid_curve_->setYAxis(QwtPlot::yLeft);
    ask_curve_->setYAxis(QwtPlot::yLeft);
  }
  bid_curve_->attach(this /*.get()*/);
  ask_curve_->attach(this /*.get()*/);
}

// ----------------------------------------------------------------------------
OrderBookPlot::~OrderBookPlot()
{
  order_book_.reset();
  book_dbg<2>.debug(str<>("Destroying"), "orderbook plot");
}

// ----------------------------------------------------------------------------
void OrderBookPlot::clearPlot()
{
  // Detach and delete any existing plot curves
  this->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
  this->detachItems(QwtPlotItem::Rtti_PlotMarker, true);
}

// ----------------------------------------------------------------------------
void OrderBookPlot::showItem(QwtPlotItem* item, bool on)
{
  item->setVisible(on);
  replot();
}

// ----------------------------------------------------------------------------
void OrderBookPlot::exportPlot()
{
  QwtPlotRenderer renderer;
  renderer.exportTo(this, "stockchart.pdf");
}

// ----------------------------------------------------------------------------
void OrderBookPlot::update_time_and_replot()
{
  QString now = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss");
  //
  QwtText axisTitleX("Price " + now);
  axisTitleX.setRenderFlags(Qt::AlignRight | Qt::AlignVCenter);
  axisTitleX.setFont(axis_title_font);
  this->setAxisTitle(QwtPlot::xBottom, axisTitleX);
  //
  this->replot();
}

// ----------------------------------------------------------------------------
void OrderBookPlot::update_graph_limits()
{
  bool primary = true;
  //
  int index = primary ? 0 : 1;
  auto [minx, maxx] = order_book_->get_xminmax(primary);
  setAxisScale(QwtPlot::xBottom, minx, maxx);
  auto [miny, maxy] = order_book_->get_yminmax(primary);
  setAxisScale(QwtPlot::yLeft, miny, maxy);
}

// ----------------------------------------------------------------------------
void OrderBookPlot::new_data_event()
{
  // push this data into the graph object
  orderbook_lock lock = order_book_->take_bid_ask_lock();
  const auto& [bids, asks] = order_book_->get_bidask_data();
  bid_curve_->setRawSamples_locked(bids.rate, bids.total);
  ask_curve_->setRawSamples_locked(asks.rate, asks.total);
}
