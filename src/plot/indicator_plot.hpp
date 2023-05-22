#pragma once

// Qwt
#include <QwtPlot>
#include <QwtScaleDraw>
#include <QwtText>
// Grox
#include "plot/ohlc_interactor.hpp"
//
class ohlc_chart_curve;
//class ohlc_price_scaledraw;
class ohlc_picker;
//
class QwtDateScaleDraw;
class QwtDateScaleEngine;
class QwtPlotDirectPainter;
class QwtPlotCurve;
class QwtPlotItem;
class QwtPlotTextLabel;
class QwtTextLabel;

// ----------------------------------------------------------------------------
class indicator_plot : public QwtPlot
{
  Q_OBJECT

  private:
  QwtDateScaleDraw* timescaleDraw_;
  QwtDateScaleEngine* timescaleEngine_;

  public:
  indicator_plot(QWidget*);
  ~indicator_plot();
  //
  void update_time_axis(double t1, double t2);
  void add_asset_curve(const QString& title, const QVector<QPointF>& samples, const QColor& color);

  private Q_SLOTS:
  void showItem(QwtPlotItem*, bool on);
};
