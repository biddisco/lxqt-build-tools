#pragma once

#include <QVector>
//
#include <qwt_plot_tradingcurve.h>

class OHLCCurve : public QwtPlotTradingCurve
{
public:
    OHLCCurve(const QVector<QwtOHLCSample> &chartData);
};
