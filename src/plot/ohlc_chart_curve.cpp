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
    setSymbolExtent(0.8 * ohlc_chart_data::minute);
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
    if (to < 0) to = dataSize() - 1;
    if (from < 0) from = 0;
    if (from > to) return;

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

    const QRectF tr = QwtScaleMap::invTransform(xMap, yMap, canvasRect);
    double tMin = tr.left();
    double tMax = tr.right();

    const bool doAlign = QwtPainter::roundingAlignment(painter);

    double symbolWidth = scaledSymbolWidth(xMap, yMap, canvasRect);
    if (doAlign)
        symbolWidth = std::floor(0.5 * symbolWidth) * 2.0;

    // find the min/max indices that we need to iterate over
    ohlc_chart_data const *chartData = dynamic_cast<ohlc_chart_data const *>(data());
    from = std::max(int64_t(0), chartData->sample_index(tMin));
    to   = std::min(int64_t(chartData->data().size()-1), chartData->sample_index(tMax));

    for (int i = from; i <= to; i++)
    {
        const QwtOHLCSample &s = sample(i);
        QwtOHLCSample translatedSample;
        int brushIndex;

        if (symbolStyleCopy == ohlc_chart_curve::HeikinAshi) {
            // first point in plot needs a prev open/close
            if (start_heikin) {
                // for first iteration, we need previous open/close
                if (i==from && i>0) {
                    const QwtOHLCSample &prev = sample(i-1);
                    prev_open  = prev.open;
                    prev_close = prev.close;
                }
                else if (i==from) {
                    prev_open  = s.open;
                    prev_close = s.close;
                }
                start_heikin = false;
            }

            double close = 0.25 * (s.open + s.high + s.low + s.close);
            double open  = 0.50 * (prev_open + prev_close);
            double high  = std::max(std::max(s.open, s.close), s.high);
            double low   = std::min(std::min(s.open, s.close), s.low);

            // next candle will use this open/close
            prev_open  = open;
            prev_close = close;

            brushIndex = (open < close)
                    ? QwtPlotTradingCurve::Increasing
                    : QwtPlotTradingCurve::Decreasing;

            translatedSample.time = xMap.transform(s.time);
            translatedSample.open = yMap.transform(open);
            translatedSample.high = yMap.transform(high);
            translatedSample.low = yMap.transform(low);
            translatedSample.close = yMap.transform(close);

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
    const QRectF tr = QwtScaleMap::invTransform(xMap, yMap, canvasRect);
    double tMin = tr.left();
    double tMax = tr.right();

    const bool doAlign = QwtPainter::roundingAlignment(painter);

    double symbolWidth = scaledSymbolWidth(xMap, yMap, canvasRect);
    if (doAlign)
        symbolWidth = std::floor(0.5 * symbolWidth) * 2.0;

    // find the min/max indices that we need to iterate over
    ohlc_chart_data const *chartData = dynamic_cast<ohlc_chart_data const *>(data());
    from = std::max(int64_t(0), chartData->sample_index(tMin));
    to   = std::min(int64_t(chartData->data().size()-1), chartData->sample_index(tMax));

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
