#pragma once

#include <QwtPlot>
#include <QwtPlotItem>
#include <QwtDateScaleDraw>
//
#include "src/plot/PlotInteractor.hpp"

class data_holder;

class CryptoPricePlot: public QwtPlot
{
    Q_OBJECT

private:
    data_holder      *data_holder_;
    PlotInteractor   *PlotInteractor_;
    QwtDateScaleDraw *timescaleDraw_;

public:
    CryptoPricePlot(QWidget *, data_holder *);
    //
    void set_data(data_holder *data_holder);

    void setupWheelZooming();

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
