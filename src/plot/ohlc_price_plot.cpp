// STL
#include <iostream>
#include <sstream>
#include <cassert>
// Qt
#include <QDateTime>
#include <QDebug>
#include <QFontDatabase>
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
#include <QwtPlotCurve>
#include <QwtSymbol>
// Grox
#include "src/plot/ohlc_chart_data.hpp"
#include "src/plot/ohlc_price_plot.hpp"
#include "src/plot/ohlc_date_scaledraw.hpp"
#include "src/plot/ohlc_interactor.hpp"
#include "src/plot/ohlc_picker.hpp"
#include "src/plot/ohlc_chart_curve.hpp"
#include "src/print.hpp"
//
#include <range/v3/view.hpp>

// ----------------------------------------------------------------------------
// Just a simple override to make the number of decimals consistent
class ohlc_price_scaledraw : public QwtScaleDraw
{
    int dec_;
public:
    ohlc_price_scaledraw(int N) : QwtScaleDraw(), dec_(N) {}

    QwtText label(double value) const QWT_OVERRIDE {
        return QString::number(value, 'f', dec_);
    }
};

// ----------------------------------------------------------------------------
ohlc_price_plot::ohlc_price_plot(QWidget *parent, ohlc_dataset_manager *data)
    : QwtPlot(parent)
    , plot_interactor_(nullptr)
    , timescaleDraw_(nullptr)
    , timescaleEngine_(nullptr)
    , direct_painter_(nullptr)
    , ohlc_dataset_manager_(data)
    , auto_candle_resolution_(true)
{
    QwtText text(" ");
    text.setColor(Qt::lightGray);
    setTitle(text);

    // Small label we use to show current candle resolution
    candle_label_ = new QwtTextLabel(this);
    candle_label_->setIndent(4);
    candle_label_->setMargin(8);

    candle_status_ = new QwtTextLabel(this);
    candle_status_->setIndent(48);
    candle_status_->setMargin(8);
    candle_status_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // find a fix font char size
    QString X = "X";
    fixed_char_size_x_ = QFontMetrics(candle_status_->font()).tightBoundingRect(X).width();
    fixed_char_size_y_ = QFontMetrics(candle_status_->font()).tightBoundingRect(X).height();
    candle_status_->setGeometry(48, 0, 66*fixed_char_size_x_, 4*fixed_char_size_y_);

    // default start up resolution
    candle_resolution_ = ohlc_chart_data::minute;

    // find difference between local time and UTC, for 'correct' date/time axis
    QDateTime local(QDateTime::currentDateTime());
    QDateTime UTC(local.toUTC());
    QDateTime dt(UTC.date(), UTC.time(), Qt::LocalTime);

    // X axis : setup date/time axis scaling and tick draw
    timescaleDraw_   = new ohlc_date_scaledraw(Qt::TimeSpec::OffsetFromUTC);
    timescaleEngine_ = new QwtDateScaleEngine(Qt::TimeSpec::OffsetFromUTC);
    timescaleDraw_->setUtcOffset(dt.secsTo(local));
    timescaleEngine_->setUtcOffset(dt.secsTo(local));

    // Adjust the RHS of the X axis. Otherwise, there is space on the RHS.
    timescaleEngine_->setAttribute(QwtScaleEngine::Floating, true);
    setAxisScaleDraw(QwtPlot::xBottom, timescaleDraw_);
    setAxisScaleEngine(QwtPlot::xBottom, timescaleEngine_);
    setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignCenter | Qt::AlignBottom);

    // Y axis : setup price axis scaling and tick draw
    // NB : We do not need to explicitly set a left Y axis
    // the default axis can be used even when not visible
    pricescaleDraw_ = new ohlc_price_scaledraw(4);
    setAxisScaleDraw(QwtPlot::yRight, pricescaleDraw_);

    // No auto scaling - we do scaling in the interactor zoom/pan class
    setAxisAutoScale(QwtPlot::yLeft,   false);
    setAxisAutoScale(QwtPlot::yRight,  false);
    setAxisAutoScale(QwtPlot::xBottom, false);
    //
    setAxisVisible(QwtAxis::YLeft,  false);
    setAxisVisible(QwtAxis::YRight, true);

    // bring the graph slightly inside the borders to leave a small outer margin
    setContentsMargins(2, 2, 2, 2);

    // A custom interactor for zooming/panning
    plot_interactor_ = new ohlc_interactor(this, ohlc_dataset_manager_);

    // Custom crosshairs to show current cursor pos
    crosshairs_ = new ohlc_picker(canvas());

    // Attach a dotted-line grid to the plot
    QwtPlotGrid *grid = new QwtPlotGrid();
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
    palette1.setColor(QPalette::WindowText, Qt::lightGray); // ticks
    palette1.setColor(QPalette::Text, Qt::lightGray);	    // tick labels
    axisWidget(Axis::xBottom)->setPalette(palette1);

    // yr axis colours
    QPalette palette2 = axisWidget(Axis::yRight)->palette();
    palette2.setColor(QPalette::WindowText, Qt::lightGray); // ticks
    palette2.setColor(QPalette::Text, Qt::lightGray);	    // tick labels
    axisWidget(Axis::yRight)->setPalette(palette2);

    // Override the Qt size policy. Otherwise, the plot may not scale to
    // the desired dimensions from the grid layout.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumSize(0,0);
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
    // qDebug() << "New data " << new_sample.open << "\n";

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
        0, ohlc_dataset_manager_->get_live_data()->size() - 1);
}

// ----------------------------------------------------------------------------
bool ohlc_price_plot::adjust_candle_size(double res)
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
                if (r==last_auto_res) return false;
                res = last_auto_res = r;
                //
                break;
            }
        }
    }
    else {
        last_auto_res = 0;
    }
    // user selected resolution
    for (const auto &r : ohlc_chart_data::available_resolutions()) {
        auto *data = ohlc_dataset_manager_->get_dataset(r);
        data->ohlc_curve_->setSymbolExtent(0.8 * r);
        data->ohlc_curve_->setVisible(r==res);
        if (r==res) {
            QwtText candle_label(r.name_);
            candle_label.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
            if (auto_candle_resolution())
                candle_label.setColor(Qt::magenta);
            else
                candle_label.setColor(Qt::red);
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
        QwtPlotTradingCurve *curve =
            static_cast<QwtPlotTradingCurve *>(curves[i]);
        curve->setSymbolStyle(symbolStyle);
    }

    replot();
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::showItem(QwtPlotItem *item, bool on)
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
void ohlc_price_plot::update_time_axis(double t1, double t2)
{
    const bool doAutoReplot = autoReplot();
    setAutoReplot(false);

    // update the X axis with new min max
    setAxisScale(QwtAxis::XBottom, t1, t2);

    // find the min/max price for this new range
    auto minmax = ohlc_dataset_manager_->get_min_max_window(
                get_candle_resolution(), t1, t2, 0.05);

    // update the Y price axis with min max
    setAxisScale(QwtAxis::YRight, minmax.min_price_, minmax.max_price_);

    // update the Y volume axis with min max
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);

    // scale change might trigger a candle resolution update
    if (auto_candle_resolution()) {
        // when candle resolution changes, the high/low values of candles do not
        // change, so the scale is ok, but the volume bars are wrong, so
        // recompute the volume min/max if the candle size changes
        updateAxes();
        if (adjust_candle_size(0)) {
            minmax = ohlc_dataset_manager_->get_min_max_window(
                        get_candle_resolution(), t1, t2, 0.05);
            setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
        }
    }

    setAutoReplot(doAutoReplot);
    replot();
    emit plotScaleChanged(t1, t2);
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::adjust_data_scaling()
{
    const double t1 = axisScaleDiv(QwtAxis::XBottom).lowerBound();
    const double t2 = axisScaleDiv(QwtAxis::XBottom).upperBound();
    auto minmax = ohlc_dataset_manager_->get_min_max_window(
                get_candle_resolution(), t1, t2, 0.05);
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::display_candle_status(double time)
{
    int64_t index = -1;
    auto *dataset = ohlc_dataset_manager_->get_dataset(get_candle_resolution());
    if (dataset->ohlc_samples_->size()>0) {
        index = dataset->ohlc_samples_->sample_index(time);
    }
    if (index<0 || size_t(index)>=dataset->ohlc_samples_->size()) {
        candle_status_->setText("");
        return;
    }
    //
    const QwtOHLCSample &sample = dataset->ohlc_samples_->data().at(index);

    std::string c;
    if (sample.open<=sample.close)
        c = "green";
    else
        c = "red";

    static const char *html1 = "<font color=\"white\">";
    static const char *html2 = "</font> <font color=\"";
    static const char *html3 = "\">";
    std::stringstream temp;
    temp << html1 << "O: " << html2 << c << html3 << hpx::debug::fp<5,9>(sample.open)
         << html1 << "H: " << html2 << c << html3 << hpx::debug::fp<5,9>(sample.high)
         << html1 << "L: " << html2 << c << html3 << hpx::debug::fp<5,9>(sample.low)
         << html1 << "C: " << html2 << c << html3 << hpx::debug::fp<5,9>(sample.close)
         << html1 << "V: " << html2 << c << html3 << hpx::debug::fp<2,14>(sample.volume);

    QwtText status(temp.str().c_str());
    status.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
//    QColor cc("#333333");
//    status.setBorderPen(QPen(cc, 2));
//    cc.setAlpha(200);
//    status.setBackgroundBrush(cc);
    candle_status_->setText(status);
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::add_buy_sell_curve(const QString& title,
    const QVector<QPointF>& samples, const QColor& color)
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
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::add_price_curve(const QString& title,
    const QVector<QPointF>& samples, const QColor& color)
{
    auto m_curve = new QwtPlotCurve(title);
    m_curve->setYAxis(QwtPlot::yRight);
    m_curve->setRenderHint(QwtPlotItem::RenderAntialiased);
    m_curve->setStyle(QwtPlotCurve::Lines);
    m_curve->setLegendAttribute(QwtPlotCurve::LegendShowSymbol);
    m_curve->setPen(color, 2);

//    QwtSymbol* symbol = new QwtSymbol(QwtSymbol::XCross);
//    symbol->setSize(16);
//    symbol->setPen(color, 4);
//    m_curve->setSymbol(symbol);

    m_curve->setSamples(samples);
    m_curve->attach(this);
}
