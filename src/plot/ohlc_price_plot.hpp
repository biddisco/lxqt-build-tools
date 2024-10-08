#pragma once

#include <memory>
// Qwt
#include <QwtPlot>
// Grox
#include "currency/ohlctv_sample.hpp"
#include "data/timebased_chart_data.hpp"
#include "plot/timebased_chart_plot.hpp"

class ohlc_chart_curve;
class ohlc_dataset_view;
class ohlc_interactor;
class ohlc_price_scaledraw;
class ohlc_picker;
class timebased_data_curve;
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
class QwtPlotDirectPainter;
class QwtPlotCurve;
class QwtPlotItem;
class QwtTextLabel;

class ohlc_price_plot : public timebased_chart_plot
{
  Q_OBJECT

  private:
  ohlc_price_scaledraw* pricescaleDraw_;
  QwtPlotDirectPainter* direct_painter_;
  std::shared_ptr<ohlc_dataset_view> ohlc_dataset_view_;
  // a map of datasets, key is resolution
  std::map<double, ohlc_chart_curve*> curves_;
  std::map<double, ohlc_chart_curve*> live_curves_;
  QwtTextLabel* candle_label_;
  QwtTextLabel* candle_status_;
  double candle_resolution_;
  bool auto_candle_resolution_;
  int fixed_char_size_x_;
  int fixed_char_size_y_;
  bool first_update_;
  double last_auto_res_;

  using timebased_chart_plot::crosshairs_;
  using timebased_chart_plot::plot_interactor_;
  using timebased_chart_plot::timescaleDraw_;
  using timebased_chart_plot::timescaleEngine_;

  public:
  ohlc_price_plot(QWidget*, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_);
  ~ohlc_price_plot();
  //
  void bind_graphs();
  void update_live_data(ohlctv_sample const& new_sample);
  //
  bool adjust_candle_size(double res);
  double get_candle_resolution() { return candle_resolution_; }
  bool auto_candle_resolution() { return auto_candle_resolution_; }
  void set_auto_candle_resolution(bool a) { auto_candle_resolution_ = a; }

  // recomputes min/max for price/volue, recomputes candles sizes etc
  void update_time_axis(double t1, double t2, bool emit_signal = false) override;
  double quantize_x_coord(double x) override;
  // when the picker moves, we find the current candle and display info
  void display_picker_info(const QPointF pos) override;

  // when candle resolution changes, the volume bar min/max must be updated
  void adjust_data_scaling();
  bool update_candle_size();

  timebased_data_curve* add_overlay_curve(
      QString const& title, point_chart_data* data, QColor const& color);

  timebased_data_curve* add_overlay_volume_curve(
      QString const& title, point_chart_data* data, QColor const& color);

  QwtPlotCurve* add_buy_sell_curve(
      QString const& title, QVector<QPointF> const& samples, QColor const& color);

  void updateLayout() override;

  public Q_SLOTS:
  void setMode(int);
  void exportPlot();

  private Q_SLOTS:
  void showItem(QwtPlotItem*, bool on);
};
