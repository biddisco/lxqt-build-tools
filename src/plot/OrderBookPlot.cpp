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
constexpr int debug_level = 0;
//
template <int Level>
static print_threshold<Level, debug_level> book_dbg("ord-plot");

// ----------------------------------------------------------------------------
OrderBookPlot::OrderBookPlot(QWidget* parent)
  : QwtPlot(parent)
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
  QPalette palette3 = axisWidget(Axis::yRight)->palette();
  palette3.setColor(QPalette::WindowText, Qt::darkYellow);    // tick
  palette3.setColor(QPalette::Text, Qt::darkMagenta);         // tick labels
  axisWidget(Axis::yRight)->setPalette(palette3);

  QwtText axisTitleY2("XRPL");
  axisTitleY2.setRenderFlags(Qt::AlignRight | Qt::AlignTop);
  axisTitleY2.setFont(axis_title_font);
  this->setAxisTitle(QwtPlot::yRight, axisTitleY2);

  enableAxis(QwtPlot::yRight);
}

// ----------------------------------------------------------------------------
OrderBookPlot::~OrderBookPlot()
{
  // dummy destructor;
  book_dbg<0>.debug(str<>("Destroying"), "orderbook plot");
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
  QString now = QDateTime::currentDateTime().toUTC().toString("yyyy-MM-dd hh:mm:ss");
  //
  QwtText axisTitleX("Price " + now);
  axisTitleX.setRenderFlags(Qt::AlignRight | Qt::AlignVCenter);
  axisTitleX.setFont(axis_title_font);
  this->setAxisTitle(QwtPlot::xBottom, axisTitleX);
  //
  this->replot();
}
