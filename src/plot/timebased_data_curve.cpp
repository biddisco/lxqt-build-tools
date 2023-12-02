// Qt
#include <QList>
#include <QPainter>
#include <QPen>
#include <QString>
// Qwt
#include <QwtPlotCurve>
#include <QwtPlotItem>
#include <QwtScaleMap>
// Grox
#include "data/timebased_chart_data.hpp"
#include "plot/timebased_data_curve.hpp"

// ----------------------------------------------------------------------------
timebased_data_curve::timebased_data_curve(QString const& title)
  : QwtPlotCurve(title)
{
  setRenderHint(QwtPlotItem::RenderAntialiased, true);

  // Don't display pattern items in the legend
  setItemAttribute(QwtPlotItem::Legend, false);

  // don't fit a curve to the data
  setCurveAttribute(QwtPlotCurve::Fitted, false);
}

// ----------------------------------------------------------------------------
void timebased_data_curve::drawSeries(QPainter* painter, QwtScaleMap const& xMap,
  QwtScaleMap const& yMap, QRectF const& canvasRect, int from, int to) const
{
  // find the min/max indices that we need to iterate over,
  // add +1 to min to clip 1 inside at the left of the x axis
  // right hand side is trucated by int conversion and always clipped anyway
  timebased_chart_data<QPointF> const* time_data =
    dynamic_cast<timebased_chart_data<QPointF> const*>(data());
  if (time_data->size() == 0)
  {
    return;
  }

  if (!compute_time_limits(time_data, xMap, yMap, canvasRect, from, to))
  {
    return;
  }

  QwtPlotCurve::drawSeries(painter, xMap, yMap, canvasRect, from, to);
}
