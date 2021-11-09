#include <cmath>
// Qt
#include <QPen>
#include <QPainter>
// Qwt
#include <QwtScaleMap>
#include <QwtDate>
#include <QwtPainter>
#include <QwtMath>
// Grox
#include "src/plot/ohlc_chart_curve.hpp"

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
void ohlc_chart_curve::drawSeries(QPainter*painter,
    const QwtScaleMap& xMap, const QwtScaleMap& yMap,
    const QRectF& canvasRect, int from, int to) const
{
    // Heikin Ashi uses a movingg average, so the first point needs special treatment
    start_heikin = true;

    // Overridden draw function to handle Heikin Ashi plot
    QwtPlotTradingCurve::drawSeries(painter, xMap, yMap, canvasRect, from, to);
}

// ----------------------------------------------------------------------------
// Not used since we only plot indices that are known to be in the right range
//static inline bool qwtIsSampleInside(const QwtOHLCSample& sample,
//    double tMin, double tMax, double vMin, double vMax)
//{
//    const double t = sample.time;
//    const QwtInterval interval = sample.boundingInterval();

//    const bool isOffScreen = (t < tMin) || (t > tMax)
//        || (interval.maxValue() < vMin) || (interval.minValue() > vMax);

//    return !isOffScreen;
//}

// ----------------------------------------------------------------------------
// Note that the to draw Heikin Ashi candles, we do not need to override the
// DrawUSerSymbol method, because the candles are the same shape/size as normal
// candles -we just need to modifiy the open/close/low/high vars to handle the
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

    const QwtScaleMap* timeMap, * valueMap;
    double tMin, tMax, vMin, vMax;

    const Qt::Orientation orient = orientation();
    if (orient == Qt::Vertical)
    {
        timeMap = &xMap;
        valueMap = &yMap;

        tMin = tr.left();
        tMax = tr.right();
        vMin = tr.top();
        vMax = tr.bottom();
    }
    else
    {
        timeMap = &yMap;
        valueMap = &xMap;

        vMin = tr.left();
        vMax = tr.right();
        tMin = tr.top();
        tMax = tr.bottom();
    }

    const bool inverted = timeMap->isInverting();
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
                prev_open  = s.open;
                prev_close = s.close;
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

            translatedSample.time = timeMap->transform(s.time);
            translatedSample.open = valueMap->transform(open);
            translatedSample.high = valueMap->transform(high);
            translatedSample.low = valueMap->transform(low);
            translatedSample.close = valueMap->transform(close);

        }
        else {
            brushIndex = (s.open < s.close)
                ? QwtPlotTradingCurve::Increasing
                : QwtPlotTradingCurve::Decreasing;
            translatedSample.time = timeMap->transform(s.time);
            translatedSample.open = valueMap->transform(s.open);
            translatedSample.high = valueMap->transform(s.high);
            translatedSample.low = valueMap->transform(s.low);
            translatedSample.close = valueMap->transform(s.close);
        }

        QPen pen = symbolPenCopy[brushIndex];
        pen.setCapStyle(Qt::FlatCap);

        painter->setPen(pen);

        if (doAlign)
        {
            translatedSample.time = qRound(translatedSample.time);
            translatedSample.open = qRound(translatedSample.open);
            translatedSample.high = qRound(translatedSample.high);
            translatedSample.low = qRound(translatedSample.low);
            translatedSample.close = qRound(translatedSample.close);
        }

        switch(symbolStyleCopy)
        {
            case Bar:
            {
                drawBar(painter, translatedSample,
                    orient, inverted, symbolWidth);
                break;
            }
            case CandleStick:
            {
                painter->setBrush(symbolBrushCopy[ brushIndex ]);
                drawCandleStick(painter, translatedSample,
                    orient, symbolWidth);
                break;
            }
            case HeikinAshi:
            {
                painter->setBrush(symbolBrushCopy[ brushIndex ]);
                drawCandleStick(painter, translatedSample,
                    orient, symbolWidth);
                break;
            }
            default:
            {
                if (symbolStyleCopy >= UserSymbol)
                {
                    painter->setBrush(symbolBrushCopy[ brushIndex ]);
                    drawUserSymbol(painter, SymbolStyle(symbolStyleCopy),
                        translatedSample, orient, inverted, symbolWidth);
                }
            }
        }
    }
}
