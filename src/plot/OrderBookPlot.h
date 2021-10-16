#pragma once

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
//
#include "src/plot/OrderBookCurve.h"

class OrderBookPlot: public QwtPlot
{
    Q_OBJECT

private:


public:
    OrderBookPlot( QWidget * = NULL );
    ~OrderBookPlot();
    //
    void clearPlot();

    QFont axis_title_font;

    static const int bid_ask_max = 200;

    // data arrays
    double xData[bid_ask_max];
    double yData[bid_ask_max];
    OrderBookCurve *plot_curve_;


public Q_SLOTS:
    void update_time_and_replot();
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
