// STL
#include <sstream>
#include <cassert>
// Qt
#include <QDateTime>
#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
// Qwt
#include <QwtDateScaleDraw>
#include <QwtDateScaleEngine>
#include <QwtPlot>
#include <QwtPlotDirectPainter>
#include <QwtPlotGrid>
#include <QwtPlotLayout>
#include <QwtPlotLegendItem>
#include <QwtPlotRenderer>
#include <QwtScaleMap>
#include <QwtScaleWidget>
#include <QwtSeriesData>
// Grox
#include "src/plot/ohlc_chart_data.hpp"
#include "src/plot/ohlc_price_plot.hpp"
#include "src/plot/ohlc_date_scaledraw.hpp"
#include "src/plot/ohlc_interactor.hpp"
#include "src/plot/ohlc_picker.hpp"
#include "src/plot/ohlc_chart_curve.hpp"

// ----------------------------------------------------------------------------
ohlc_price_plot::ohlc_price_plot(QWidget *parent, ohlc_dataset_manager *data)
    : QwtPlot( parent )
    , ohlc_dataset_manager_(data)
    , plot_interactor_(nullptr)
    , timescaleDraw_(nullptr)
    , timescaleEngine_(nullptr)
    , ohlc_curve_(nullptr)
    , live_curve_(nullptr)
    , direct_painter_(nullptr)
{
    setTitle("XRP");

    // find difference between local time and UTC, for 'correct' date/time axis
    QDateTime local(QDateTime::currentDateTime());
    QDateTime UTC(local.toUTC());
    QDateTime dt(UTC.date(), UTC.time(), Qt::LocalTime);

    // setup date/time axis scaling and tick draw
    timescaleDraw_   = new ohlc_date_scaledraw(Qt::TimeSpec::OffsetFromUTC);
    timescaleEngine_ = new QwtDateScaleEngine(Qt::TimeSpec::OffsetFromUTC);
    timescaleDraw_->setUtcOffset(dt.secsTo(local));
    timescaleEngine_->setUtcOffset(dt.secsTo(local));
    setAxisScaleDraw( QwtPlot::xBottom, timescaleDraw_ );
    setAxisScaleEngine( QwtPlot::xBottom, timescaleEngine_ );

    // @TODO :needed? Enable autoscaling for axes
    setAxisAutoScale( QwtPlot::yLeft );
    setAxisAutoScale( QwtPlot::xBottom);

    setAxisLabelAlignment( QwtPlot::xBottom, Qt::AlignCenter | Qt::AlignBottom );

    // The following is needed to properly adjust the RHS of the X axis. Otherwise,
    // there is space on the RHS.
    axisScaleEngine(QwtPlot::xBottom)->setAttribute(QwtScaleEngine::Floating,true);

    // bring the graph slightly inside the borders to leave a small outer margin
    this->setContentsMargins( 4, 4, 4, 4 );

    // Use this to reduce the graph scale inside the inner plot area
    this->plotLayout()->setCanvasMargin( 16, QwtPlot::yRight );

    // not sure about this, no yRight axis setup
    this->axisScaleEngine(QwtPlot::yRight)->setMargins(8, 8);

    plot_interactor_ = new ohlc_interactor( this, ohlc_dataset_manager_);
    crosshairs_ = new ohlc_picker(this->canvas());

    // Attach a dotted-line grid to the plot
    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setItemAttribute(grid->Legend, false);
    grid->setPen(QColor(Qt::darkGray), 0.0, Qt::PenStyle::DotLine);
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
    static const QColor c("#18191b");

    // QWidget : fill background before painting (color = QPalette::Window)
    setAutoFillBackground( true );

    // palette for widget
    QPalette palette0 = palette();
    palette0.setColor( QPalette::Window, c);
    canvas()->setPalette(palette0);
    setPalette(palette0);

    // x axis
    QPalette palette1 = axisWidget(Axis::xBottom)->palette();
    palette1.setColor( QPalette::WindowText, Qt::lightGray); // ticks
    palette1.setColor( QPalette::Text, Qt::lightGray);	     // tick labels
    axisWidget(Axis::xBottom)->setPalette( palette1 );

    // y axis
    QPalette palette2 = axisWidget(Axis::yLeft)->palette();
    palette2.setColor( QPalette::WindowText, Qt::lightGray); // ticks
    palette2.setColor( QPalette::Text, Qt::lightGray);	     // tick labels
    axisWidget(Axis::yLeft)->setPalette( palette2 );
}

// ----------------------------------------------------------------------------
ohlc_price_plot::~ohlc_price_plot()
{
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_data_array(ohlc_dataset_manager *ohlc_dataset_manager)
{
    ohlc_dataset_manager_ = ohlc_dataset_manager;

    if (ohlc_curve_) {
        // mark data as changed
        ohlc_curve_->itemChanged();
    }
    else {
        // create a new plotting curve for OHLC data
        auto ohlc = ohlc_dataset_manager_->get_samples();
        ohlc_curve_ = new ohlc_chart_curve(ohlc);
        // bind it to this plot and turn on display
        ohlc_curve_->attach(this);
        ohlc_curve_->setVisible(true);
    }
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_live_data(QwtOHLCSample const &new_sample)
{
    qDebug() << "New data " << new_sample.open << "\n";

    ohlc_dataset_manager_->add_live_data(new_sample);
    // The live data is typically only a small number of samples
    if (!live_curve_) {
        direct_painter_ = new QwtPlotDirectPainter(this);
        live_curve_ = new ohlc_chart_curve(ohlc_dataset_manager_->get_live_samples());
        live_curve_->attach(this);
        live_curve_->setVisible(true);
    }
    else {
        live_curve_->itemChanged();
    }
    direct_painter_->drawSeries(live_curve_, 0, ohlc_dataset_manager_->get_live_samples()->size() - 1 );
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::setMode( int style )
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
void ohlc_price_plot::showItem( QwtPlotItem *item, bool on )
{
    item->setVisible( on );
    replot();
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "stockchart.pdf" );
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::adjust_candle_size()
{
}
