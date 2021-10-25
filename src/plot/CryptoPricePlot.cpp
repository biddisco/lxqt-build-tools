#include <QMouseEvent>
#include <QDebug>
#include <QWheelEvent>
//
#include <qwt_legend.h>
#include <qwt_plot_zoneitem.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_layout.h>
#include <qwt_legend_label.h>
#include <qwt_date.h>
#include <qwt_date_scale_engine.h>
#include <qwt_date_scale_draw.h>
#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_curve.h>
#include <qwt_symbol.h>
#include <qwt_legend.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_legenditem.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
//
#include <QwtDateScaleDraw>
//
#include <sstream>
#include <assert.h>

#include "src/plot/CryptoPricePlot.hpp"

#include "src/plot/StockChartDateScaleDraw.h"
#include "src/plot/StockChartPlotZoomer.h"
#include "src/plot/OHLCCurve.h"
#include "src/plot/PlotInteractor.hpp"
#include "src/util/QDateHelper.h"

// ----------------------------------------------------------------------------
CryptoPricePlot::CryptoPricePlot(QWidget *parent, data_holder *data)
    : QwtPlot( parent )
    , data_holder_(data)
    , PlotInteractor_(nullptr)
    , timescaleDraw_(nullptr)
    , ohlc_curve_(nullptr)
{
//    setTitle("XRP");

    // only need to do this on first init
    timescaleDraw_   = new QwtDateScaleDraw(Qt::TimeSpec::UTC);
    timescaleEngine_ = new QwtDateScaleEngine(Qt::TimeSpec::UTC);
    setAxisScaleDraw( QwtPlot::xBottom, timescaleDraw_ );
    setAxisScaleEngine( QwtPlot::xBottom, timescaleEngine_ );

    // Enable autoscaling for axes
    setAxisAutoScale( QwtPlot::yLeft );
    setAxisAutoScale( QwtPlot::xBottom);

    setAxisLabelRotation( QwtPlot::xBottom, -50.0 );
    setAxisLabelAlignment( QwtPlot::xBottom, Qt::AlignLeft | Qt::AlignBottom );

    // The following is needed to properly adjust the RHS of the X axis. Otherwise,
    // there is space on the RHS.
    axisScaleEngine(QwtPlot::xBottom)->setAttribute(QwtScaleEngine::Floating,true);

    // bring the graph slightly inside the borders to leave a small outer margin
    this->setContentsMargins( 4, 4, 4, 4 );

    // Use this to reduce the graph scale inside the inner plot area
    this->plotLayout()->setCanvasMargin( 0, QwtPlot::yRight );

    this->axisScaleEngine(QwtPlot::yRight)->setMargins(1000, 1000);

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

    // TODO: Check the memory ownership/leak for the following allocation
    //plotZoomer_ = new StockChartPlotZoomer( canvas() );
//    plotZoomer_->setWheelModifiers(Qt::ControlModifier);
//    plotZoomer_->setAxisEnabled(Qt::XAxis, false);
//    plotZoomer_->setAxisEnabled(Qt::YAxis, false);


    PlotInteractor *panner = new PlotInteractor( this, data_holder_);
    panner->setMouseButton(Qt::LeftButton, Qt::ShiftModifier);
    panner->setAxisEnabled(Qt::XAxis, true);
    panner->setAxisEnabled(Qt::YAxis, false);
    panner->setEnabled(true);

    // Attach a dotted-line grid to the plot.
    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setItemAttribute(grid->Legend, false);
    grid->setPen(QColor(Qt::lightGray), 0.0, Qt::PenStyle::DotLine);
    grid->attach(this);

    // Override the size policy. Otherwise, the plot may not scale to
    // the desired dimensions from the grid layout.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumSize(0,0);

    // Attach a legend internal to the plot
    QwtPlotLegendItem *legend = new QwtPlotLegendItem();
    legend->setAlignmentInCanvas(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
    legend->attach(this);

    // main canvas color - dark, but not black
    static const QColor c( 0x28, 0x28, 0x28 );

    // QWidget : fill background before painting (color = QPalette::Window)
    setAutoFillBackground( true );

    // palette for widget
    QPalette palette0 = palette();
    palette0.setColor( QPalette::Window, c);
    canvas()->setPalette(palette0);
    setPalette(palette0);

    // x axis
    QPalette palette1 = axisWidget(Axis::xBottom)->palette();
    palette1.setColor( QPalette::WindowText, Qt::lightGray); // for ticks
    palette1.setColor( QPalette::Text, Qt::lightGray);	     // for ticks' labels
    axisWidget(Axis::xBottom)->setPalette( palette1 );

    // y axis
    QPalette palette2 = axisWidget(Axis::yLeft)->palette();
    palette2.setColor( QPalette::WindowText, Qt::lightGray); // for ticks
    palette2.setColor( QPalette::Text, Qt::lightGray);	     // for ticks' labels
    axisWidget(Axis::yLeft)->setPalette( palette2 );

    setupWheelZooming();
}

// ----------------------------------------------------------------------------
//void CryptoPricePlot::clearPatternPlots()
//{
//    // Detach and delete any existing plot curves
//    this->detachItems(QwtPlotItem::Rtti_PlotCurve,true);
//    this->detachItems(QwtPlotItem::Rtti_PlotMarker,true);
//}

/*
void CryptoPricePlot::populateOnePatternShape(const PatternMatchPtr &patternMatch)
{
    PatternShapeGenerator shapeGen;
    PatternShapePtr patternShape = shapeGen.generateShape(*patternMatch);
    PatternShapePointVectorVectorPtr curveShapes = patternShape->curveShapes();


    // Re-populate with the pattern for the given patternMatch
    for(PatternShapePointVectorVector::iterator curveShapeIter = curveShapes->begin();
        curveShapeIter != curveShapes->end(); curveShapeIter++)
    {
        bool doCurveFit = true;
        QwtPlotCurve *patternMatchPlot = new PatternPlotCurve(*curveShapeIter,doCurveFit);
        patternMatchPlot->attach(this);
    }

    PatternShapePointVectorVectorPtr lineShapes = patternShape->lineShapes();
    for(PatternShapePointVectorVector::iterator lineShapeIter = lineShapes->begin();
        lineShapeIter != lineShapes->end(); lineShapeIter++)
    {
        bool doCurveFit = false;
        QwtPlotCurve *patternMatchPlot = new PatternPlotCurve(*lineShapeIter,doCurveFit);
        patternMatchPlot->attach(this);
    }


    if(patternMatch->breakoutInfo)
    {
        BreakoutPlotMarker *breakoutPlotMarker = new BreakoutPlotMarker(
                    patternMatch->breakoutInfo->pseudoXVal(),patternMatch->breakoutInfo->breakoutPrice());
        breakoutPlotMarker->attach(this);
    }
    else if(patternMatch->breakdownInfo)
    {
        BreakdownPlotMarker *breakdownPlotMarker = new BreakdownPlotMarker(
                    patternMatch->breakdownInfo->pseudoXVal(),patternMatch->breakdownInfo->breakoutPrice());
        breakdownPlotMarker->attach(this);
    }

    replot();

}
*/

//// ----------------------------------------------------------------------------
//void CryptoPricePlot::populatePatternMatchesShapes(const PatternMatchListPtr &patternMatches)
//{
//    clearPatternPlots();

//    for(PatternMatchList::iterator matchesIter = patternMatches->begin();
//        matchesIter != patternMatches->end(); matchesIter++)
//    {
////        populateOnePatternShape(*matchesIter);
//    }
//}

// ----------------------------------------------------------------------------
void CryptoPricePlot::update_data_array(data_holder *data_holder)
{
    data_holder_ = data_holder;

    // remove old plot from graph
    if (ohlc_curve_) {
        ohlc_curve_->detach();
        delete ohlc_curve_;
    }

    // create a new plotting curve for OHLC data
    // (plot data is refcounted by Qwt)
    auto ohlc = data_holder_->get_data();
    ohlc_curve_ = new OHLCCurve(ohlc);

    // bind it to this plot and turn on display
    ohlc_curve_->attach(this);
    showItem(ohlc_curve_, true);
}

// ----------------------------------------------------------------------------
void CryptoPricePlot::set_data(data_holder *data_holder)
{

    update_data_array(data_holder);


    // Update the chart data for the plot zoomer, so it can show a curser with appropriate data.
    // plotZoomer_->setChartData(instrSelInfo->chartData());

    // The following has the effect of freezing the maximum zoom coordinates to the
    // initial scale of the chart. This needs to happen after replot(). The scale
    // for zooming needs to be reset whenever the chart data changes.
//    plotZoomer_->setZoomBase(false);
//    plotZoomer_->setChartScale(timescaleDraw_);

}

// ----------------------------------------------------------------------------
//void CryptoPricePlot::populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo)
//{

//    clearPatternPlots();
//    this->detachItems(QwtPlotItem::Rtti_PlotTradingCurve,true);

//    setTitle(instrSelInfo->instrumentName());

//    QwtDateScaleDraw *scaleDraw = new StockChartDateScaleDraw( Qt::UTC,instrSelInfo->chartData() );
//    setAxisScaleDraw( QwtPlot::xBottom, scaleDraw );

//    StockChartPlotCurve *chartDataCurve = new StockChartPlotCurve(instrSelInfo->chartData());
//    chartDataCurve->attach( this );
//    showItem( chartDataCurve, true );

//    // Rescale the plot based upon the boundaries of the current chart data
//    setAxisAutoScale( QwtPlot::yLeft );
//    setAxisAutoScale( QwtPlot::xBottom);

//    // Update the chart data for the plot zoomer, so it can show a curser with appropriate data.
//    plotZoomer_->setChartData(instrSelInfo->chartData());

//    replot();

//    // The following has the effect of freezing the maximum zoom coordinates to the
//    // initial scale of the chart. This needs to happen after replot(). The scale
//    // for zooming needs to be reset whenever the chart data changes.
//    plotZoomer_->setZoomBase(false);

//}

// ----------------------------------------------------------------------------
void CryptoPricePlot::setMode( int style )
{
    QwtPlotTradingCurve::SymbolStyle symbolStyle =
        static_cast<QwtPlotTradingCurve::SymbolStyle>( style );

    QwtPlotItemList curves = itemList( QwtPlotItem::Rtti_PlotTradingCurve );
    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotTradingCurve *curve =
            static_cast<QwtPlotTradingCurve *>( curves[i] );
        curve->setSymbolStyle( symbolStyle );
    }

    replot();
}

// ----------------------------------------------------------------------------
void CryptoPricePlot::showItem( QwtPlotItem *item, bool on )
{
    item->setVisible( on );
    replot();
}

// ----------------------------------------------------------------------------
void CryptoPricePlot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "stockchart.pdf" );
}

// ----------------------------------------------------------------------------
void CryptoPricePlot::setupWheelZooming()
{
    return;
    QwtPlotPanner *pan_x = new QwtPlotPanner( canvas() );
    pan_x->setMouseButton(Qt::NoButton, Qt::ShiftModifier);
    pan_x->setAxisEnabled(Qt::XAxis, true);
    pan_x->setAxisEnabled(Qt::YAxis, false);

    QwtPlotPanner *pan_y = new QwtPlotPanner( canvas() );
    pan_y->setMouseButton(Qt::NoButton, Qt::ControlModifier);
    pan_y->setAxisEnabled(Qt::XAxis, false);
    pan_y->setAxisEnabled(Qt::YAxis, true);


    QwtPlotMagnifier *zoom_x = new QwtPlotMagnifier( canvas() );
    zoom_x->setWheelModifiers(Qt::ShiftModifier);
    zoom_x->setAxisEnabled(Qt::XAxis, false);
    zoom_x->setAxisEnabled(Qt::YAxis, true);
    zoom_x->setAxisEnabled(Qt::ZAxis, false);

    QwtPlotMagnifier *zoom_y = new QwtPlotMagnifier( canvas() );
    zoom_y->setWheelModifiers(Qt::ControlModifier);
    zoom_y->setAxisEnabled(Qt::XAxis, false);
    zoom_y->setAxisEnabled(Qt::YAxis, false);

}
