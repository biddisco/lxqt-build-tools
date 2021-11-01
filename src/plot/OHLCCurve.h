#pragma once

// Qt
#include <QVector>
// Qwt
#include <QwtPlotTradingCurve>
// Grox
#include "src/data/ohlc_data.hpp"

class OHLCCurve : public QwtPlotTradingCurve
{
public:
    using QwtPlotTradingCurve::QwtPlotTradingCurve;
    explicit OHLCCurve(OHLCData *chartData);
    explicit OHLCCurve(const QVector<QwtOHLCSample> &chartData);
    explicit OHLCCurve(const QString& title = QString());
};
