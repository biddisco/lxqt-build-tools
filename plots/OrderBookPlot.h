#pragma once

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
//
#include "plots/OrderBookCurve.h"

class OrderBookPlot: public QwtPlot
{
    Q_OBJECT

private:


public:
    OrderBookPlot( QWidget * = NULL );
    void clearPlot();

    static const int bid_ask_max = 200;

    // data arrays
    double xData[bid_ask_max];
    double yData[bid_ask_max];
    OrderBookCurve *plot_curve_;

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
