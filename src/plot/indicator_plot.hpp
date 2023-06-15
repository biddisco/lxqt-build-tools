#pragma once

// Qwt
#include <QwtPlot>
#include <QwtScaleDraw>
#include <QwtText>
// Grox
#include "plot/ohlc_interactor.hpp"
#include "plot/timebased_chart_plot.hpp"
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
class QwtPlotItem;

// ----------------------------------------------------------------------------
class indicator_plot : public timebased_chart_plot
{
  Q_OBJECT

  public:
  using timebased_chart_plot::crosshairs_;
  using timebased_chart_plot::plot_interactor_;
  using timebased_chart_plot::timescaleDraw_;
  using timebased_chart_plot::timescaleEngine_;

  indicator_plot(QWidget*);
  ~indicator_plot();
  //
  void update_time_axis(double t1, double t2) override;
  void add_asset_curve(const QString& title, const QVector<QPointF>& samples, const QColor& color);

  private Q_SLOTS:
  void showItem(QwtPlotItem*, bool on);
};
