// STL
#include <iostream>
#include <sstream>
#include <cassert>
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
#include <fmt/format.h>

// ----------------------------------------------------------------------------
using namespace grox::debug;
// a debug level of N shows messages with priority<N
constexpr int debug_level = 5;
//
template <int Level>
static print_threshold<Level, debug_level> plot_dbg("OHLCplot");

// ----------------------------------------------------------------------------
void fill_text_label(QwtText &label) {
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
        dec_= 4;

        int exponent  = range>0 ? (int)floor(log10(fabs(range))) : 0;
        double base   = (range * pow(10.0,  -1*exponent));

        // if negative we need decimal places
        if (exponent<0) {
            dec_ = 1 - exponent;
            if (dec_>4) {
                form_ = 'e';
                dec_ = 3;
                dig_ = dec_ + 6;
            }
        }
        else {
            dig_ = dec_ + 2;
        }
        fstr = fmt::format("%{}.{}{}", dig_, dec_, form_);
        plot_dbg<5>.debug(str<>("Format string"), fstr);
    }

    QwtText label(double value) const QWT_OVERRIDE {
        return QwtText(QString().asprintf(fstr.c_str(), value),
                       QwtText::TextFormat::PlainText);
    }
};

// ----------------------------------------------------------------------------
ohlc_price_plot::ohlc_price_plot(QWidget *parent, std::shared_ptr<ohlc_dataset_view> data)
    : QwtPlot(parent)
    , plot_interactor_(nullptr)
    , timescaleDraw_(nullptr)
    , timescaleEngine_(nullptr)
    , direct_painter_(nullptr)
    , ohlc_dataset_view_(data)
    , auto_candle_resolution_(true)
{
    // find a fix font char size for candle status/data
    QString X = "X";
    QFont label_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixed_char_size_x_ = QFontMetrics(label_font).tightBoundingRect(X).width();
    fixed_char_size_y_ = QFontMetrics(label_font).tightBoundingRect(X).height();
    const int margin = 0.5*fixed_char_size_y_; // margin space in x and y
    const int indent = 4; // text offset in x direction
    const int label_xtext = 3*fixed_char_size_x_; // "15d", "30m" etc
    const int label_xsize = 2*indent + 2*margin + label_xtext;
    // Small label we use to show current candle resolution
    candle_label_ = new QwtTextLabel(this);
    candle_label_->setIndent(indent);
    candle_label_->setMargin(margin);
    candle_label_->setFont(label_font);
    candle_label_->setGeometry(0, 0, label_xsize, 2*margin + 2*fixed_char_size_y_);
    // candle_label_->setFrameStyle(QFrame::Panel | QFrame::Raised);

    candle_status_ = new QwtTextLabel(this);
    candle_status_->setIndent(indent);
    candle_status_->setMargin(margin);
    candle_status_->setFont(label_font);
    // OHLCV format string : "O:<num>" = 5(OHLCV)*2 + 4(OHLC)*9 + 1(V)*14 = 61
    candle_status_->setGeometry(label_xsize, 0, 61*fixed_char_size_x_, 2*margin + 2*fixed_char_size_y_);
    // candle_status_->setFrameStyle(QFrame::Panel | QFrame::Raised);

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

    ohlcv_minmax minmax = data->get_min_max(
                ohlc_chart_data::minute,
                data->get_first_sample_time(),
                data->get_last_sample_time(false));

    // Y axis : setup price axis scaling and tick draw
    // NB : We do not need to explicitly set a left Y axis
    // the default axis can be used even when not visible
    pricescaleDraw_ = new ohlc_price_scaledraw(minmax.max_price_ - minmax.min_price_);
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
    plot_interactor_ = new ohlc_interactor(this, ohlc_dataset_view_);

    // Custom crosshairs to show current cursor pos
    crosshairs_ = new ohlc_picker(canvas());

    // Attach a dotted-line grid to the plot
    QwtPlotGrid *grid = new QwtPlotGrid();
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

    bind_graphs();
}

// ----------------------------------------------------------------------------
ohlc_price_plot::~ohlc_price_plot()
{
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::bind_graphs()
{
    auto resolutions = ohlc_dataset_view_->get_dataset_resolutions();
    bool first = true;
    for (auto r : resolutions) {
        auto *data = ohlc_dataset_view_->get_dataset(r);
        // bind it to this plot
        data->ohlc_curve_->attach(this);
        data->ohlc_curve_->setVisible(first);
        first = false;
    }
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::update_live_data(QwtOHLCSample const &new_sample)
{
    // repaint the live dataset
    if (!direct_painter_) {
        direct_painter_ = new QwtPlotDirectPainter(this);
        // qwt directpainter used inside a Qt-AdvancedDockingSystem window
        // has bad repaint effects unless we turn on CopyBackingStore
        direct_painter_->setAttribute(QwtPlotDirectPainter::FullRepaint, false);
        direct_painter_->setAttribute(QwtPlotDirectPainter::CopyBackingStore, true);
        ohlc_dataset_view_->get_live_curve()->attach(this);
        ohlc_dataset_view_->get_live_curve()->setVisible(true);
    }
    else {
        ohlc_dataset_view_->get_live_curve()->itemChanged();
    }
    direct_painter_->drawSeries(ohlc_dataset_view_->get_live_curve(),
        0, ohlc_dataset_view_->get_live_data()->size() - 1);
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
        auto *data = ohlc_dataset_view_->get_dataset(r);
        data->ohlc_curve_->setVisible(r==res);
        if (r==res) {
            data->ohlc_curve_->setSymbolExtent(0.8 * r);
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
    // recompute the mapping to/from world/pixels
    updateAxes();

    // we need the min/max price for the new time range
    ohlcv_minmax minmax;
    bool candles_changed = false;

    // if the mapping has changed a lot, we might need to change candle sizes
    // scale change might trigger a candle resolution update
    if (auto_candle_resolution()) {
        // when candle resolution changes, the high/low values of candles do not
        // change, so the scale is ok, but the volume bars are wrong, so
        // recompute the volume min/max if the candle size changes
        // recompute scaling so we can get the correct candle size
        if (adjust_candle_size(0)) {
            candles_changed = true;
            minmax = ohlc_dataset_view_->get_min_max_window(
                        get_candle_resolution(), t1, t2, 0.05);
            setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
        }
    }

    if (!candles_changed) {
        minmax = ohlc_dataset_view_->get_min_max_window(
            get_candle_resolution(), t1, t2, 0.05);
    }

    // update the Y price axis with min max
    setAxisScale(QwtAxis::YRight, minmax.min_price_, minmax.max_price_);

    // update the Y volume axis with min max
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);

    plot_dbg<5>.debug(str<>("min_max"), ohlc_chart_data::get_resolution(get_candle_resolution()).name_
                     , msecs_unix_to_calendar_time(t1)
                     , "->", msecs_unix_to_calendar_time(t2)
                     , "(", minmax.min_price_, ",", minmax.max_price_, ")");

    setAutoReplot(doAutoReplot);
    replot();
    emit plotScaleChanged(t1, t2);
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::adjust_data_scaling()
{
    const double t1 = axisScaleDiv(QwtAxis::XBottom).lowerBound();
    const double t2 = axisScaleDiv(QwtAxis::XBottom).upperBound();
    auto minmax = ohlc_dataset_view_->get_min_max_window(
                get_candle_resolution(), t1, t2, 0.05);
    setAxisScale(QwtAxis::YLeft, 0, minmax.max_volume_);
}

// ----------------------------------------------------------------------------
void ohlc_price_plot::display_candle_status(double time)
{
    int64_t index = -1;
    auto *dataset = ohlc_dataset_view_->get_dataset(get_candle_resolution());
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
    temp << html1 << " O:" << html2 << c << html3 << fp<5,9>(sample.open)
         << html1 << " H:" << html2 << c << html3 << fp<5,9>(sample.high)
         << html1 << " L:" << html2 << c << html3 << fp<5,9>(sample.low)
         << html1 << " C:" << html2 << c << html3 << fp<5,9>(sample.close)
         << html1 << " V:" << html2 << c << html3 << fp<2,14>(sample.volume);

    QwtText status(temp.str().c_str());
    status.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
    //fill_text_label(status);
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

// ----------------------------------------------------------------------------
void ohlc_price_plot::updateLayout()
{
    QwtPlot::updateLayout();
    //
    static bool was_visible = isVisible();
    //
    if (isVisible() && was_visible != isVisible()) {
        plot_dbg<5>.debug(str<>("updateLayout"), "adjust_candle_size");
        if (auto_candle_resolution()) {
            adjust_candle_size(0);
        }
        else {
            adjust_candle_size(get_candle_resolution());
        }
        was_visible=isVisible();
    }
}

// ----------------------------------------------------------------------------
