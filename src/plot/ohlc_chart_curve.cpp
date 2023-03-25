#include <cmath>
// Qt
#include <QPen>
#include <QPainter>
// Qwt
#include <QwtScaleMap>
#include <QwtDate>
#include <QwtPainter>
#include <QwtMath>
#include <QwtPlot>
// Grox
#include "src/plot/ohlc_chart_curve.hpp"
#include "src/data/ohlc_heikin_ashi.hpp"

constexpr double volume_reduction = 0.25;

// ----------------------------------------------------------------------------
ohlc_chart_curve::ohlc_chart_curve(ohlc_chart_data *chartData)
    : QwtPlotTradingCurve()
{
    setData(chartData);

    setTitle("");
    setItemAttribute(QwtPlotItem::Legend, false); // Legend not needed for price plot
    setOrientation(Qt::Vertical);

    // overridden if/when scale/resolution changes
    setSymbolExtent(0.8 * ohlc_data_resolutions::minute);
    setMinSymbolWidth(1);
    setMaxSymbolWidth(0.0);

    // Bitstamp colors : Darkish green "#159f49", Darkish red "#df4249"
    setSymbolPen(QwtPlotTradingCurve::Increasing, QColor("#159f49"));
    setSymbolPen(QwtPlotTradingCurve::Decreasing, QColor("#df4249"));
    setSymbolBrush(QwtPlotTradingCurve::Increasing, QColor("#159f49"));
    setSymbolBrush(QwtPlotTradingCurve::Decreasing, QColor("#df4249"));

    // Bitstamp Heikin Ashi : Darkish green "#26a69a", Darkish red "#ef5350"
    setSymbolPenHA(QwtPlotTradingCurve::Increasing, QColor("#26a69a"));
    setSymbolPenHA(QwtPlotTradingCurve::Decreasing, QColor("#ef5350"));
    setSymbolBrushHA(QwtPlotTradingCurve::Increasing, QColor("#26a69a"));
    setSymbolBrushHA(QwtPlotTradingCurve::Decreasing, QColor("#ef5350"));

    // Bitstamp colors : Darkish green "#159f49", Darkish red "#df4249"
    setSymbolPenVolume(QwtPlotTradingCurve::Increasing, QColor("#17472b"));
    setSymbolPenVolume(QwtPlotTradingCurve::Decreasing, QColor("#5e272b"));
    setSymbolBrushVolume(QwtPlotTradingCurve::Increasing, QColor("#17472b"));
    setSymbolBrushVolume(QwtPlotTradingCurve::Decreasing, QColor("#5e272b"));

    setYAxis(QwtPlot::yRight);

}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::setSymbolPenHA(Direction d, const QPen &p)
{
    HAPen[d] = p;
}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::setSymbolBrushHA(Direction d, const QBrush &b)
{
    HABrush[d] = b;
}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::setSymbolPenVolume(Direction d, const QPen &p)
{
    VolumePen[d] = p;
}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::setSymbolBrushVolume(Direction d, const QBrush &b)
{
    VolumeBrush[d] = b;
}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::drawSeries(QPainter*painter,
    const QwtScaleMap& xMap, const QwtScaleMap& yMap,
    const QRectF& canvasRect, int from, int to) const
{
    // find the min/max indices that we need to iterate over,
    // add +1 to min to clip 1 inside at the left of the x axis
    // right hand side is trucated by int conversion and always clipped anyway
    ohlc_chart_data const *chartData = dynamic_cast<ohlc_chart_data const *>(data());
    if (chartData->size()==0) {
        return;
    }
    const QRectF tr = QwtScaleMap::invTransform(xMap, yMap, canvasRect);
    double tMin = tr.left();
    double tMax = tr.right();
    from = std::max(int64_t(0), chartData->sample_index(tMin)+1);
    to   = std::min(int64_t(chartData->data().size()-1), chartData->sample_index(tMax));
    from = std::min(from, to);
    to   = std::max(from, to);

//    if (to < 0) to = dataSize() - 1;
//    if (from < 0) from = 0;
//    if (from > to) return;

    painter->save();

    // draw volume first so that candles are always visible over the top
    auto plot_ = plot();
    QwtScaleMap ymap = plot_->canvasMap(QwtPlot::yLeft);
    drawVolume(painter, xMap, ymap, canvasRect, from, to);

    // Heikin Ashi uses a movingg average, so the first point needs special treatment
    start_heikin = true;

    if (symbolStyle() != QwtPlotTradingCurve::NoSymbol)
        drawSymbols(painter, xMap, yMap, canvasRect, from, to);

    painter->restore();
}

// ----------------------------------------------------------------------------
// Note that to draw Heikin Ashi candles, we do not need to override the
// DrawUSerSymbol method, because the candles are the same shape/size as normal
// candles - we just need to modifiy the open/close/low/high vars to handle the
// averaging as we iterate.
void ohlc_chart_curve::drawSymbols(QPainter* painter,
                                   const QwtScaleMap& xMap, const QwtScaleMap& yMap,
                                   const QRectF& canvasRect, int from, int to) const
{
    // some vars are private the the Qwt Trading plot, so we must make copies
    QPen symbolPenCopy[2];
    QBrush symbolBrushCopy[2];
    int symbolStyleCopy = symbolStyle();
    if (symbolStyleCopy == ohlc_chart_curve::HeikinAshi) {
        symbolPenCopy[Direction::Increasing] = HAPen[Direction::Increasing];
        symbolPenCopy[Direction::Decreasing] = HAPen[Direction::Decreasing];
        symbolBrushCopy[Direction::Increasing] = HABrush[Direction::Increasing];
        symbolBrushCopy[Direction::Decreasing] = HABrush[Direction::Decreasing];
    }
    else {
        symbolPenCopy[Direction::Increasing] = symbolPen(Direction::Increasing);
        symbolPenCopy[Direction::Decreasing] = symbolPen(Direction::Decreasing);
        symbolBrushCopy[Direction::Increasing] = symbolBrush(Direction::Increasing);
        symbolBrushCopy[Direction::Decreasing] = symbolBrush(Direction::Decreasing);
    }

    const bool doAlign = QwtPainter::roundingAlignment(painter);

    double symbolWidth = scaledSymbolWidth(xMap, yMap, canvasRect);
    if (doAlign)
        symbolWidth = std::floor(0.5 * symbolWidth) * 2.0;

    // initialize heikin ashi functor
    const QwtOHLCSample &init_ha = sample(from>0 ? (from-1) : from);
    ohlc_heikin_ashi heikin_ashi(init_ha);

    for (int i = from; i <= to; i++)
    {
        const QwtOHLCSample &s = sample(i);
        QwtOHLCSample translatedSample;
        int brushIndex;

        if (symbolStyleCopy == ohlc_chart_curve::HeikinAshi) {

            const QwtOHLCSample ha = heikin_ashi(s).value();

            brushIndex = (ha.open < ha.close)
                    ? QwtPlotTradingCurve::Increasing
                    : QwtPlotTradingCurve::Decreasing;

            translatedSample.time = xMap.transform(s.time);
            translatedSample.open = yMap.transform(ha.open);
            translatedSample.high = yMap.transform(ha.high);
            translatedSample.low = yMap.transform(ha.low);
            translatedSample.close = yMap.transform(ha.close);
        }
        else {
            brushIndex = (s.open < s.close)
                    ? QwtPlotTradingCurve::Increasing
                    : QwtPlotTradingCurve::Decreasing;
            translatedSample.time = xMap.transform(s.time);
            translatedSample.open = yMap.transform(s.open);
            translatedSample.high = yMap.transform(s.high);
            translatedSample.low = yMap.transform(s.low);
            translatedSample.close = yMap.transform(s.close);
        }

        QPen pen = symbolPenCopy[brushIndex];
        pen.setCapStyle(Qt::FlatCap);
        pen.setWidthF(0.1*symbolWidth);

        painter->setPen(pen);

        if (doAlign)
        {
            translatedSample.time = qRound(translatedSample.time);
            translatedSample.open = qRound(translatedSample.open);
            translatedSample.high = qRound(translatedSample.high);
            translatedSample.low = qRound(translatedSample.low);
            translatedSample.close = qRound(translatedSample.close);
        }

        painter->setBrush(symbolBrushCopy[ brushIndex ]);
        drawCandleStick(painter, translatedSample,
                        Qt::Orientation::Vertical, symbolWidth);
    }
}

// ----------------------------------------------------------------------------
// Note that to draw Heikin Ashi candles, we do not need to override the
// DrawUSerSymbol method, because the candles are the same shape/size as normal
// candles - we just need to modifiy the open/close/low/high vars to handle the
// averaging as we iterate.
void ohlc_chart_curve::drawVolume(QPainter* painter,
                                  const QwtScaleMap& xMap, const QwtScaleMap& yMap,
                                  const QRectF& canvasRect, int from, int to) const
{
    const bool doAlign = QwtPainter::roundingAlignment(painter);

    double symbolWidth = scaledSymbolWidth(xMap, yMap, canvasRect);
    if (doAlign)
        symbolWidth = std::floor(0.5 * symbolWidth) * 2.0;

    for (int i = from; i <= to; i++)
    {
        const QwtOHLCSample &s = sample(i);

        int brushIndex = (s.open <= s.close)
                ? QwtPlotTradingCurve::Increasing
                : QwtPlotTradingCurve::Decreasing;

        double translatedTime = xMap.transform(s.time);
        double translatedV0   = yMap.transform(0.0);
        double translatedV1   = yMap.transform(s.volume * volume_reduction);

        if (doAlign)
        {
            translatedTime = qRound(translatedTime);
            translatedV0   = qRound(translatedV0);
            translatedV1   = qRound(translatedV1);
        }

        QPen pen = VolumePen[brushIndex];
        pen.setCapStyle(Qt::FlatCap);
        painter->setPen(pen);
        painter->setBrush( VolumeBrush[brushIndex] );


        const QwtOHLCSample translatedSample(translatedTime,
            0.0, translatedV1, translatedV0, 0.0, 0.0);

        drawVolumeBar(painter, translatedSample, symbolWidth);
    }
}

// ----------------------------------------------------------------------------
void ohlc_chart_curve::drawVolumeBar( QPainter* painter,
    const QwtOHLCSample& sample, double width ) const
{
    QRectF rect( sample.time - 0.5 * width,
                 sample.low,
                 width,
                 sample.high - sample.low);
    QwtPainter::drawRect( painter, rect );
}
