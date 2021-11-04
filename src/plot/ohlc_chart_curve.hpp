#pragma once

// Qt
#include <QVector>
// Qwt
#include <QwtPlotTradingCurve>
// Grox
#include "src/plot/ohlc_chart_data.hpp"

class ohlc_chart_curve : public QwtPlotTradingCurve
{
public:
    using QwtPlotTradingCurve::QwtPlotTradingCurve;
    explicit ohlc_chart_curve(ohlc_chart_data *chartData);
    explicit ohlc_chart_curve(const QVector<QwtOHLCSample> &chartData);
    explicit ohlc_chart_curve(const QString& title = QString());
};
