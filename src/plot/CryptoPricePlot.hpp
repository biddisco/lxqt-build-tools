#pragma once

#include <QwtPlot>
#include <QwtPlotItem>
//
#include "src/plot/PlotInteractor.hpp"

class data_holder;
class OHLCCurve;
class QwtDateScaleDraw;
class QwtDateScaleEngine;

class CryptoPricePlot: public QwtPlot
{
    Q_OBJECT

private:
    data_holder        *data_holder_;
    PlotInteractor     *plot_interactor_;
    QwtDateScaleDraw   *timescaleDraw_;
    QwtDateScaleEngine *timescaleEngine_;
    OHLCCurve          *ohlc_curve_;

public:
    CryptoPricePlot(QWidget *, data_holder *);
    //
    void update_data_array(data_holder *data_holder);
    //
    void adjust_candle_size();

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
