#pragma once

#include <memory>
// Qwt
#include <QwtPlot>
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
//
class ohlc_picker;
class ohlc_interactor;

class timebased_chart_plot : public QwtPlot
{
  Q_OBJECT

  protected:
  ohlc_interactor* plot_interactor_;
  ohlc_picker* crosshairs_;
  QwtDateScaleDraw* timescaleDraw_;
  QwtDateScaleEngine* timescaleEngine_;

  public:
  timebased_chart_plot(QWidget* parent)
    : QwtPlot(parent)
    , plot_interactor_(nullptr)
    , crosshairs_(nullptr)
    , timescaleDraw_(nullptr)
    , timescaleEngine_(nullptr)
  {
  }

  // recomputes min/max for price/volue, recomputes candles sizes etc
  virtual void update_time_axis(double t1, double t2){};

  // clamps the x/time axis value to the valid sample point resolution
  virtual double quantize_x_coord(double x)
  {
    return x;
  };

  virtual void display_picker_info(const QPointF pos){};

  ohlc_interactor* get_interactor()
  {
    return plot_interactor_;
  }
  ohlc_picker* get_crosshairs()
  {
    return crosshairs_;
  }
};
