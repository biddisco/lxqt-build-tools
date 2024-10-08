
#include <iostream>
//
#include <QMouseEvent>
#include <QwtInterval>
//
#include "plot/ohlc_picker.hpp"
#include "plot/timebased_chart_plot.hpp"

void timebased_chart_plot::onCrossHairsMoved(QPointF const& pos)
{
  // use provided x (time) value, insert y midpoint from our yaxis
  // to make sure selected point is onscreen
  auto interval = axisInterval(QwtPlot::yRight);
  auto ymid = 0.5 * (interval.minValue() + interval.maxValue());

  QPointF new_pos(pos.x(), ymid);

  const QwtPlotItemList curves = itemList(QwtPlotItem::Rtti_PlotCurve);
  if (curves.size() > 0)
  {
    // QPointF pos = crosshairs_->invTransform(crosshairs_->trackerPosition());

    const QLineF line =
        crosshairs_->curveLineAt(static_cast<const QwtPlotCurve*>(curves[0]), pos.x());
    if (!line.isNull())
    {
      const double curveY = line.pointAt((pos.x() - line.p1().x()) / line.dx()).y();
      new_pos.setY(curveY);
      // r.moveBottom(pos.y());
    }
  }
  QPointF device_pos = crosshairs_->transform(new_pos);

  //  const QwtScaleMap xmap = canvasMap(QwtAxis::XBottom);
  //  const QwtScaleMap ymap = canvasMap(QwtAxis::YRight);
  //  QPointF devicePos(xmap.transform(pos.x()), pos.y() /*ymap.transform(ymid)*/);

  // fake a mouse move event to cause crosshairs to be drawn
  QMouseEvent mouseEvent(QEvent::Type::MouseMove, device_pos, Qt::MouseButton::NoButton,
      Qt::MouseButton::NoButton, Qt::KeyboardModifier::NoModifier);
  crosshairs_->injectMouseMoveEvent(&mouseEvent);
}
