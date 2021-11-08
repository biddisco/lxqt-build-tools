#pragma once

// Qt
#include <QVector>
#include <QPen>
#include <QBrush>
// Qwt
#include <QwtPlotTradingCurve>
// Grox
#include "src/plot/ohlc_chart_data.hpp"

class ohlc_chart_curve : public QwtPlotTradingCurve
{
public:
    enum GrixSymbolStyle : int
    {
        HeikinAshi = QwtPlotTradingCurve::SymbolStyle::UserSymbol + 1
    };

public:
    using QwtPlotTradingCurve::QwtPlotTradingCurve;
    explicit ohlc_chart_curve(ohlc_chart_data *chartData);

    void drawSeries( QPainter*,
        const QwtScaleMap& xMap, const QwtScaleMap& yMap,
        const QRectF& canvasRect, int from, int to ) const QWT_OVERRIDE;

    void drawSymbols( QPainter* painter,
        const QwtScaleMap& xMap, const QwtScaleMap& yMap,
        const QRectF& canvasRect, int from, int to ) const QWT_OVERRIDE;

    void setSymbolPenHA(Direction, const QPen&);
    void setSymbolBrushHA(Direction, const QBrush&);

    QPen    HAPen[2];
    QBrush  HABrush[2];
    mutable bool   start_heikin;
    mutable double prev_open;
    mutable double prev_close;
};
