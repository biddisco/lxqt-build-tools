#include <qwt_date.h>
#include <qwt_date_scale_draw.h>
#include <qwt_date_scale_engine.h>
#include <qwt_legend.h>
#include <qwt_legend_label.h>
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_legenditem.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_tradingcurve.h>
#include <qwt_plot_zoneitem.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>
//
#include "plots/OrderBookCurve.h"
#include "plots/OrderBookPlot.h"
#include <iostream>

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
    QwtPlotLegendItem *legend = new QwtPlotLegendItem();
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
    palette1.setColor(QPalette::WindowText, Qt::lightGray); // ticks
    palette1.setColor(QPalette::Text, Qt::lightGray);       // tick labels
    axisWidget(Axis::xBottom)->setPalette(palette1);

    QwtText axisTitleX( "Price" );
    axisTitleX.setRenderFlags( Qt::AlignRight | Qt::AlignVCenter );
    axisTitleX.setFont( axis_title_font );
    this->setAxisTitle( QwtPlot::xBottom, axisTitleX );

    // y axis
    QPalette palette2 = axisWidget(Axis::yLeft)->palette();
    palette2.setColor(QPalette::WindowText, Qt::green);    // ticks
    palette2.setColor(QPalette::Text, Qt::red);            // tick labels
    axisWidget(Axis::yLeft)->setPalette(palette2);

    QwtText axisTitleY( "Bitstamp" );
    axisTitleY.setRenderFlags( Qt::AlignLeft | Qt::AlignTop );
    axisTitleY.setFont( axis_title_font );
    this->setAxisTitle( QwtPlot::yLeft, axisTitleY );

    // 2nd y axis
    QPalette palette3 = axisWidget(Axis::yRight)->palette();
    palette3.setColor(QPalette::WindowText, Qt::darkYellow); // tick
    palette3.setColor(QPalette::Text, Qt::darkMagenta);      // tick labels
    axisWidget(Axis::yRight)->setPalette(palette3);

    QwtText axisTitleY2( "XRPL" );
    axisTitleY2.setRenderFlags( Qt::AlignRight | Qt::AlignTop );
    axisTitleY2.setFont( axis_title_font );
    this->setAxisTitle( QwtPlot::yRight, axisTitleY2 );

    enableAxis(QwtPlot::yRight);
}

OrderBookPlot::~OrderBookPlot()
{
    // dummy destructor;
    std::cout << "Destroying orderbook plot" << std::endl;
}

void OrderBookPlot::clearPlot()
{
    // Detach and delete any existing plot curves
    this->detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    this->detachItems(QwtPlotItem::Rtti_PlotMarker, true);
}

void OrderBookPlot::setMode(int style)
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

void OrderBookPlot::showItem(QwtPlotItem* item, bool on)
{
    item->setVisible(on);
    replot();
}

void OrderBookPlot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo(this, "stockchart.pdf");
}

void OrderBookPlot::update_time_and_replot()
{
    QString now = QDateTime::currentDateTime().toUTC().toString("yyyy-MM-dd hh:mm:ss");
    //
    QwtText axisTitleX( "Price " + now);
    axisTitleX.setRenderFlags( Qt::AlignRight | Qt::AlignVCenter );
    axisTitleX.setFont( axis_title_font );
    this->setAxisTitle( QwtPlot::xBottom, axisTitleX );
    //
    this->replot();
}
