#pragma once

#include <QVector>
//
#include <qwt_plot_tradingcurve.h>

class OHLCCurve : public QwtPlotTradingCurve
{
public:
    using QwtPlotTradingCurve::QwtPlotTradingCurve;
    explicit OHLCCurve(const QVector<QwtOHLCSample> &chartData);
    explicit OHLCCurve(const QString& title = QString());
};
