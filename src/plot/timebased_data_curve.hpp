#pragma once

// Qt
#include <QPainter>
#include <QPen>
#include <QString>
// Qwt
#include <QwtPlotCurve>
#include <QwtScaleMap>
//
#include <data/timebased_chart_data.hpp>

// ----------------------------------------------------------------------------
template <typename T>
bool compute_time_limits(timebased_chart_data<T> const* time_data, QwtScaleMap const& xMap,
  QwtScaleMap const& yMap, QRectF const& canvasRect, int& from, int& to)
{
  const QRectF tr = QwtScaleMap::invTransform(xMap, yMap, canvasRect);
  double time_Min = tr.left();
  double time_Max = tr.right();
  from = std::max(int64_t(0), time_data->sample_index(time_Min) + 1);
  to = std::min(int64_t(time_data->data().size() - 1), time_data->sample_index(time_Max));
  from = std::min(from, to);
  to = std::max(from, to);
  //
  return true;
}

// ----------------------------------------------------------------------------
class timebased_data_curve : public QwtPlotCurve
{
  public:
  explicit timebased_data_curve(const QString& title = QString());

  void drawSeries(QPainter*, QwtScaleMap const& xMap, QwtScaleMap const& yMap,
    QRectF const& canvasRect, int from, int to) const QWT_OVERRIDE;
};
