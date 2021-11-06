// STL
#include <iostream>
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
#include <QwtPlotTextLabel>
#include <QwtTextLabel>
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
//
#include <range/v3/view.hpp>
// ----------------------------------------------------------------------------
ohlc_price_plot::ohlc_price_plot(QWidget *parent, ohlc_dataset_manager *data)
    : QwtPlot( parent )
    , plot_interactor_(nullptr)
    , timescaleDraw_(nullptr)
    , timescaleEngine_(nullptr)
    , direct_painter_(nullptr)
    , ohlc_dataset_manager_(data)
    , auto_candle_resolution_(true)
{
    setTitle("XRP");

    // default start up resolution
    candle_resolution_ = ohlc_chart_data::minute;

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

    candle_label_ = new QwtTextLabel(this);
    candle_label_->setMargin(0);
}

// ----------------------------------------------------------------------------
ohlc_price_plot::~ohlc_price_plot()
{
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::set_data(ohlc_dataset_manager *ohlc_dataset_manager)
{
    ohlc_dataset_manager_ = ohlc_dataset_manager;
    auto resolutions = ohlc_dataset_manager_->get_dataset_resolutions();
    bool first = true;
    for (auto r : resolutions) {
        auto *data = ohlc_dataset_manager_->get_dataset(r);
        // bind it to this plot
        data->ohlc_curve_->attach(this);
        data->ohlc_curve_->setVisible(first);
        first = false;
    }
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_live_data(QwtOHLCSample const &new_sample)
{
    qDebug() << "New data " << new_sample.open << "\n";

    ohlc_dataset_manager_->add_live_data(new_sample);
    // The live data is typically only a small number of samples
    if (!direct_painter_) {
        direct_painter_ = new QwtPlotDirectPainter(this);
        ohlc_dataset_manager_->get_live_curve()->attach(this);
        ohlc_dataset_manager_->get_live_curve()->setVisible(true);
    }
    else {
        ohlc_dataset_manager_->get_live_curve()->itemChanged();
    }
    direct_painter_->drawSeries(ohlc_dataset_manager_->get_live_curve(),
        0, ohlc_dataset_manager_->get_live_data()->size() - 1 );
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::adjust_candle_size(double res)
{
    // to track the last auto change we made
    static double last_auto_res = 0;

    // auto mode
    if (res==0) {
        // if we don't find a usable coarser resolution, use 1m
        res = ohlc_chart_data::minute;
        // get the pixel/plot coordinate transform
        const QwtScaleMap map = canvasMap(QwtAxis::XBottom);
        // try for candle around ~10 pixels - How big in world coords?
        double xm = map.invTransform(10) - map.invTransform(0);
        for (const auto &r : ranges::views::reverse(ohlc_chart_data::available_resolutions())) {
            if (r<xm) {
                // if we have not changed value, just exit
                if (r==last_auto_res) return;
                res = last_auto_res = r;
                //
                break;
            }
        }
    }
    // user selected resolution
    for (const auto &r : ohlc_chart_data::available_resolutions()) {
        auto *data = ohlc_dataset_manager_->get_dataset(r);
        data->ohlc_curve_->setSymbolExtent(0.8 * r);
        data->ohlc_curve_->setVisible(r==res);
        if (r==res) {
            QwtText candle_label(r.name_);
            candle_label.setRenderFlags( Qt::AlignLeft | Qt::AlignTop );
            if (auto_candle_resolution())
                candle_label.setColor(Qt::magenta);
            else
                candle_label.setColor(Qt::red);
            candle_label_->setText(candle_label);
        }
    }
    candle_resolution_ = res;

    replot();
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
    renderer.exportTo( this, "grox.pdf" );
}
