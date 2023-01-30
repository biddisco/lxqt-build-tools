#pragma once

// Qwt
#include <QwtPlot>
// Grox
#include "src/plot/ohlc_interactor.hpp"
//
class ohlc_dataset_view;
class ohlc_chart_curve;
class ohlc_price_scaledraw;
class ohlc_picker;
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
class QwtPlotDirectPainter;
class QwtPlotCurve;
class QwtPlotItem;
class QwtPlotTextLabel;
class QwtTextLabel;

class ohlc_price_plot: public QwtPlot
{
    Q_OBJECT

private:
    ohlc_interactor               *plot_interactor_;
    ohlc_price_scaledraw          *pricescaleDraw_;
    QwtDateScaleDraw              *timescaleDraw_;
    QwtDateScaleEngine            *timescaleEngine_;
    QwtPlotDirectPainter          *direct_painter_;
    ohlc_picker                   *crosshairs_;
    std::shared_ptr<ohlc_dataset_view> ohlc_dataset_view_;
    QwtTextLabel                  *candle_label_;
    QwtTextLabel                  *candle_status_;
    double                         candle_resolution_;
    bool                           auto_candle_resolution_;
    int                            fixed_char_size_x_;
    int                            fixed_char_size_y_;
    bool                           first_update_;
    double                         last_auto_res_;

public:
    ohlc_price_plot(QWidget *, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_);
    ~ohlc_price_plot();
    //
    void bind_graphs();
    void update_live_data(QwtOHLCSample const &new_sample);
    //
    bool   adjust_candle_size(double res);
    double get_candle_resolution() {return candle_resolution_; }
    bool   auto_candle_resolution() { return auto_candle_resolution_; }
    void   set_auto_candle_resolution(bool a) { auto_candle_resolution_ = a; }

    // recomputes min/max for price/volue, recomputes candles sizes etc
    void   update_time_axis(double t1, double t2);

    // when candle resolution changes, the volume bar min/max must be updated
    void   adjust_data_scaling();
    bool   update_candle_size();

    // when the picker moves, we find the current candle and display info
    void   display_candle_status(double time);

    ohlc_interactor *get_interactor() { return plot_interactor_; }
    ohlc_picker     *get_crosshairs() { return crosshairs_; }

    void add_price_curve(const QString& title,
        const QVector<QPointF>& samples, const QColor& color);

    void add_buy_sell_curve(const QString& title,
        const QVector<QPointF>& samples, const QColor& color);

    void updateLayout() override;

signals:
    void plotScaleChanged(double t1, double t2);

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};
