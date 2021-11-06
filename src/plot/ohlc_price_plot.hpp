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
    ohlc_interactor               *plot_interactor_;
    QwtDateScaleDraw              *timescaleDraw_;
    QwtDateScaleEngine            *timescaleEngine_;
    QwtPlotDirectPainter          *direct_painter_;
    QwtPlotPicker                 *crosshairs_;
    ohlc_dataset_manager          *ohlc_dataset_manager_;
    double                         candle_resolution_;
    bool                           auto_candle_resolution_;

public:
    ohlc_price_plot(QWidget *, ohlc_dataset_manager *);
    ~ohlc_price_plot();
    //
    void set_data(ohlc_dataset_manager *ohlc_dataset_manager);
    void update_data_array();
    void update_live_data(QwtOHLCSample const &new_sample);
    //
    void adjust_candle_size(double res);
    double get_candle_resolution() {return candle_resolution_; }
    bool auto_candle_resolution() { return auto_candle_resolution_; }
    bool set_auto_candle_resolution(bool a) { auto_candle_resolution_=a; }

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
