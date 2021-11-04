#pragma once

// Qwt
#include <QwtPlot>
#include <QwtPlotItem>
// Grox
#include "src/plot/ohlc_interactor.hpp"

class ohlc_dataset_manager;
class ohlc_chart_curve;
class QwtDateScaleDraw;
class QwtDateScaleEngine;
class QwtPlotDirectPainter;
class QwtPlotCurve;
class QwtPlotPicker;

class ohlc_price_plot: public QwtPlot
{
    Q_OBJECT

private:
    ohlc_dataset_manager            *ohlc_dataset_manager_;
    ohlc_interactor        *plot_interactor_;
    QwtDateScaleDraw       *timescaleDraw_;
    QwtDateScaleEngine     *timescaleEngine_;
    ohlc_chart_curve       *ohlc_curve_;
    ohlc_chart_curve       *live_curve_;
    QwtPlotDirectPainter   *direct_painter_;
    QwtPlotPicker          *crosshairs_;

public:
    ohlc_price_plot(QWidget *, ohlc_dataset_manager *);
    ~ohlc_price_plot();
    //
    void update_data_array(ohlc_dataset_manager *ohlc_dataset_manager);
    void update_live_data(QwtOHLCSample const &new_sample);
    //
    void adjust_candle_size();

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
