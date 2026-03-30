#pragma once

#include <memory>
// Qwt
#include <QwtPlot>
//
#include "debug/logging.hpp"
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
//
class ohlc_picker;
class ohlc_interactor;

// ----------------------------------------------------------------------------
static auto chart_log = grox::log::create("chartdata");

// ----------------------------------------------------------------------------

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

  ~timebased_chart_plot() { GROX_LOG_DEBUG(chart_log, "{:<20}", "timebased_chart_plot dtor"); }

  // recomputes min/max for price/volue, recomputes candles sizes etc
  virtual void update_time_axis(double t1, double t2, bool emit_signal = false) {};

  // clamps the x/time axis value to the valid sample point resolution
  virtual double quantize_x_coord(double x) { return x; }

  virtual void display_picker_info(QPointF const pos) {};

  ohlc_interactor* get_interactor() { return plot_interactor_; }
  ohlc_picker* get_crosshairs() { return crosshairs_; }

  Q_SIGNALS:
  void timeAxisChanged(double, double, bool);

  public slots:
  void onCrossHairsMoved(QPointF const&);
};
