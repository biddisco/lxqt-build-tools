
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
#include <qwt_scale_widget.h>
//
#include <assert.h>

#include "PriceAndPatternPlot.h"

#include "StockChartDateScaleDraw.h"
#include "StockChartPlotZoomer.h"
#include "OHLCCurve.h"
#include "QDateHelper.h"

//#include "PeriodValSegment.h"
//#include "DoubleBottomScanner.h"
//#include "PatternShapeGenerator.h"
//#include "MultiPatternScanner.h"
//#include "PatternMatchFilter.h"
//#include "SymetricTriangleScanner.h"
#include <sstream>
#include <qwt_scale_engine.h>

PriceAndPatternPlot::PriceAndPatternPlot( QWidget *parent ):
    QwtPlot( parent )
{
    setTitle( "" );

    QwtLinearScaleEngine *scaleEngine = new QwtLinearScaleEngine(10);

    setAxisScaleEngine( QwtPlot::xBottom, scaleEngine );
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
    plotZoomer_ = new StockChartPlotZoomer( canvas() );
//    plotZoomer_->setWheelModifiers(Qt::ControlModifier);
//    plotZoomer_->setAxisEnabled(Qt::XAxis, false);
//    plotZoomer_->setAxisEnabled(Qt::YAxis, false);


//    QwtPlotPanner *panner = new QwtPlotPanner( canvas() );
//    panner->setMouseButton(Qt::LeftButton, Qt::ShiftModifier);
//    panner->setAxisEnabled(Qt::XAxis, true);
//    panner->setAxisEnabled(Qt::YAxis, false);

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

void PriceAndPatternPlot::clearPatternPlots()
{
    // Detach and delete any existing plot curves
    this->detachItems(QwtPlotItem::Rtti_PlotCurve,true);
    this->detachItems(QwtPlotItem::Rtti_PlotMarker,true);
}

/*
void PriceAndPatternPlot::populateOnePatternShape(const PatternMatchPtr &patternMatch)
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

void PriceAndPatternPlot::populatePatternMatchesShapes(const PatternMatchListPtr &patternMatches)
{
    clearPatternPlots();

    for(PatternMatchList::iterator matchesIter = patternMatches->begin();
        matchesIter != patternMatches->end(); matchesIter++)
    {
//        populateOnePatternShape(*matchesIter);
    }
}

void PriceAndPatternPlot::set_OHLC_data(const QVector<QwtOHLCSample> &ohlc)
{
    // remove any pattern plots
    clearPatternPlots();

    // remove the old chart data curve and delete it
    this->detachItems(QwtPlotItem::Rtti_PlotTradingCurve, true);

    setTitle("XRP");

    //    QwtDateScaleDraw *scaleDraw = new StockChartDateScaleDraw( Qt::UTC, instrSelInfo->chartData() );
    //    setAxisScaleDraw( QwtPlot::xBottom, scaleDraw );

    // only need to do this on first init
    if (!timescaleDraw_) {
        timescaleDraw_ = new QwtDateScaleDraw/*StockChartDateScaleDraw*/(Qt::TimeSpec::UTC);
        setAxisScaleDraw( QwtPlot::xBottom, timescaleDraw_ );
    }

    // create a new plotting curve for OHLC data
    OHLCCurve *chartDataCurve = new OHLCCurve(ohlc);

    // bind it to this plot and turn on display
    chartDataCurve->attach( this );
    showItem( chartDataCurve, true );

    // Rescale the plot based upon the boundaries of the current chart data
    setAxisAutoScale( QwtPlot::yLeft );
    setAxisAutoScale( QwtPlot::xBottom);

    // Update the chart data for the plot zoomer, so it can show a curser with appropriate data.
    // plotZoomer_->setChartData(instrSelInfo->chartData());

    replot();

    // The following has the effect of freezing the maximum zoom coordinates to the
    // initial scale of the chart. This needs to happen after replot(). The scale
    // for zooming needs to be reset whenever the chart data changes.
    plotZoomer_->setZoomBase(false);
    plotZoomer_->setChartScale(timescaleDraw_);

}

//void PriceAndPatternPlot::populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo)
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

void PriceAndPatternPlot::setMode( int style )
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

void PriceAndPatternPlot::showItem( QwtPlotItem *item, bool on )
{
    item->setVisible( on );
    replot();
}

void PriceAndPatternPlot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "stockchart.pdf" );
}

void PriceAndPatternPlot::setupWheelZooming()
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
