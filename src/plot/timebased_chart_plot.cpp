
#include <iostream>
//
#include <QMouseEvent>
#include <QwtInterval>
//
#include "plot/ohlc_picker.hpp"
#include "plot/timebased_chart_plot.hpp"

void timebased_chart_plot::onCrossHairsMoved(const QPointF& pos)
{
  // use provided x (time) value, insert y midpoint from our yaxis
  // to make sure selected point is onscreen
  auto interval = axisInterval(QwtPlot::yRight);
  auto ymid = 0.5 * (interval.minValue() + interval.maxValue());

  const QwtScaleMap xmap = canvasMap(QwtAxis::XBottom);
  const QwtScaleMap ymap = canvasMap(QwtAxis::YRight);
  QPointF devicePos(xmap.transform(pos.x()), ymap.transform(ymid));

  // fake a mouse move event to cause crosshairs to be drawn
  QMouseEvent mouseEvent(QEvent::Type::MouseMove, devicePos, Qt::MouseButton::NoButton,
    Qt::MouseButton::NoButton, Qt::KeyboardModifier::NoModifier);
  crosshairs_->injectMouseMoveEvent(&mouseEvent);
}
