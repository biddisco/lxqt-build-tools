#pragma once

// Qt
#include <QDateTime>
#include <QLocale>
#include <QPen>
#include <QPoint>
// Qwt
#include <QwtAxis>
#include <QwtPickerMachine>
#include <QwtPlot>
#include <QwtPlotPicker>
#include <QwtScaleEngine>
#include <QwtScaleMap>
#include <QwtScaleWidget>
#include <QwtText>
#include <QwtTextLabel>
//
#include "plot/timebased_chart_plot.hpp"

class ohlc_picker : public QwtPlotPicker
{
  public:
  mutable QPointF last_coord_;
  QwtTextLabel* yaxis_label_;
  QwtTextLabel* date_label_;

  ohlc_picker(QWidget* canvas)
    : QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YRight, canvas)
    , last_coord_(0, 0)
    , yaxis_label_(new QwtTextLabel(canvas->parentWidget()))
    , date_label_(new QwtTextLabel(canvas->parentWidget()))
  {
    timebased_chart_plot* plot_ = dynamic_cast<timebased_chart_plot*>(canvas->parentWidget());

    setTrackerMode(QwtPlotPicker::ActiveOnly);
    setRubberBand(
      QwtPicker::RubberBand(int(QwtPicker::HLineRubberBand) + int(QwtPicker::VLineRubberBand)));
    setStateMachine(new QwtPickerTrackerMachine());

    // pale blue "#9589cf"
    setRubberBandPen(QPen(QBrush("#9589cf"), 1, Qt::DashLine));
    setTrackerPen(QPen(Qt::darkGray));
  }

  QPointF last_coord()
  {
    return last_coord_;
  }

  double quantize_x_coord(const double pos) const
  {
    // get the pixel/plot coordinate transform
    timebased_chart_plot* plot_ = dynamic_cast<timebased_chart_plot*>(canvas()->parentWidget());
    if (!plot_)
      return pos;
    //
    return plot_->quantize_x_coord(pos);
  }

  QPointF quantize_x_screencoord(const QPointF& pos) const
  {
    // get the pixel/plot coordinate transform
    timebased_chart_plot* plot_ = dynamic_cast<timebased_chart_plot*>(canvas()->parentWidget());
    if (!plot_)
      return pos;
    //
    const QwtScaleMap map = plot_->canvasMap(QwtAxis::XBottom);
    double p1 = map.invTransform(pos.x());
    p1 = plot_->quantize_x_coord(p1);
    p1 = map.transform(p1);
    return QPointF(p1, pos.y());
  }

  virtual QwtText trackerTextF(const QPointF& pos) const QWT_OVERRIDE
  {
    double p1 = quantize_x_coord(pos.x());
    last_coord_ = QPointF(p1, pos.y());
    //
    return QwtText();
  }

  QPolygon adjustedPoints(const QPolygon& points) const QWT_OVERRIDE
  {
    QPolygon adjusted;
    // we only handle hLine so far
    if (points.size() == 1)
    {
      // Map the x coord to the chart, snap it to a bin,
      // then invert the mapping
      adjusted += QPoint(quantize_x_screencoord(points[0]).x(), points[0].y());
    }
    return adjusted;
  }

  virtual void updateDisplay() QWT_OVERRIDE
  {
    QwtPlotPicker::updateDisplay();

    if (!yaxis_label_)
      return;

    // -------------------------------------------------
    // Right Y axis widget (such as price)
    //
    timebased_chart_plot* plot_ = dynamic_cast<timebased_chart_plot*>(canvas()->parentWidget());
    QwtScaleWidget* yaw = plot_->axisWidget(QwtAxis::YRight);
    auto yawg = yaw->geometry();

    // Right Y axis scaling mapper
    const QwtScaleMap ymap = plot_->canvasMap(QwtAxis::YRight);
    // plot coords -> pixel coords
    auto y = ymap.transform(last_coord_.y());

    //
    // display price inside price axis
    //
    const QwtScaleDraw* ydraw = plot_->axisScaleDraw(QwtAxis::YRight);
    QwtText yaxis_text = ydraw->label(last_coord_.y());
    QColor c("#555555");
    c.setAlpha(200);
    yaxis_text.setColor(Qt::white);
    yaxis_text.setBorderPen(QPen(c, 1));
    yaxis_text.setBackgroundBrush(c);
    yaxis_text.setLayoutAttribute(QwtText::LayoutAttribute::MinimumLayout, true);
    yaxis_text.setRenderFlags(Qt::AlignLeft | Qt::AlignVCenter);

    // get size of text that will be drawn
    auto s = yaxis_text.textSize();
    // position the label
    yaxis_label_->setText(yaxis_text);
    auto g = yaxis_label_->geometry();

    g.moveTo(
      yawg.x() + ydraw->maxTickLength() + ydraw->spacing() - 1, yawg.y() + y - s.height() - 8 / 2);
    yaxis_label_->setGeometry(g.x(), g.y(), s.width() + 4, s.height() + 8);

    // -------------------------------------------------
    // Bottom X axis widget (date)
    //
    QwtScaleWidget* xaw = plot_->axisWidget(QwtAxis::XBottom);
    auto xawg = xaw->geometry();

    // Bottom X axis scaling mapper
    const QwtScaleMap xmap = plot_->canvasMap(QwtAxis::XBottom);
    double px = quantize_x_coord(last_coord_.x());
    auto x = xmap.transform(px);

    //
    // display date inside date axis
    //
    const QwtScaleDraw* xdraw = plot_->axisScaleDraw(QwtAxis::XBottom);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(px);
    QString str2 = QLocale().toString(dt, "dd-MM-yy hh:mm");
    QwtText date_text(str2);
    date_text.setColor(Qt::white);
    date_text.setBorderPen(QPen(c, 1));
    date_text.setBackgroundBrush(c);
    date_text.setLayoutAttribute(QwtText::LayoutAttribute::MinimumLayout, true);
    date_text.setRenderFlags(Qt::AlignHCenter | Qt::AlignVCenter);

    // get size of text that will be drawn
    s = date_text.textSize();
    // position the label
    date_label_->setText(date_text);
    g = date_label_->geometry();

    g.moveTo(
      xawg.x() + x - s.width() / 2, xawg.y() + xdraw->maxTickLength() + xdraw->spacing() - 1);
    date_label_->setGeometry(g.x(), g.y(), s.width() + 4, s.height() + 8);

    //
    // display the stats of the candle under the cursor
    //
    plot_->display_picker_info(last_coord_);
  }

  void widgetMouseMoveEvent(QMouseEvent* mouseEvent) override
  {
    setRubberBand(
      QwtPicker::RubberBand(int(QwtPicker::HLineRubberBand) + int(QwtPicker::VLineRubberBand)));
    QwtPicker::widgetMouseMoveEvent(mouseEvent);
  }

  void injectMouseMoveEvent(QMouseEvent* mouseEvent)
  {
    setRubberBand(QwtPicker::RubberBand(int(QwtPicker::VLineRubberBand)));
    QwtPicker::widgetMouseMoveEvent(mouseEvent);
  }
};
