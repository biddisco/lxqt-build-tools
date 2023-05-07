#pragma once

// Qt
#include <QDateTime>
#include <QPen>
#include <QPoint>
#include <QLocale>
// Qwt
#include <QwtAxis>
#include <QwtPlot>
#include <QwtPlotPicker>
#include <QwtPickerMachine>
#include <QwtScaleEngine>
#include <QwtScaleMap>
#include <QwtScaleWidget>
#include <QwtText>
#include <QwtTextLabel>
//
#include "plot/ohlc_price_plot.hpp"

class ohlc_picker : public QwtPlotPicker
{
public:
    mutable QPointF last_coord_;
    QwtTextLabel  *price_label_;
    QwtTextLabel  *date_label_;

    ohlc_picker(QWidget* canvas)
        : QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YRight, canvas)
        , last_coord_(0,0)
        , price_label_(new QwtTextLabel(canvas->parentWidget()))
        , date_label_(new QwtTextLabel(canvas->parentWidget()))
    {
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas->parentWidget());
        QwtScaleWidget* aw = plot_->axisWidget(QwtAxis::YRight);

        setTrackerMode(QwtPlotPicker::ActiveOnly);
        setRubberBand(
                    QwtPicker::RubberBand(int(QwtPicker::HLineRubberBand) + int(QwtPicker::VLineRubberBand)));
        setStateMachine(new QwtPickerTrackerMachine());

        // pale blue "#9589cf"
        setRubberBandPen(QPen(QBrush("#9589cf"), 1, Qt::DashLine));
        setTrackerPen(QPen(Qt::darkGray));
    }

    QPointF last_coord() {
        return last_coord_;
    }

    double quantize_x_coord(const double pos) const
    {
        // get the pixel/plot coordinate transform
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        if (!plot_) return pos;
        //
        double res = plot_->get_candle_resolution();
        double p1 = res * static_cast<uint64_t>((pos+res/2.0)/res);
        return p1;
    }

    QPointF quantize_x_screencoord(const QPointF& pos) const
    {
        // get the pixel/plot coordinate transform
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        if (!plot_) return pos;
        //
        const QwtScaleMap map = plot_->canvasMap(QwtAxis::XBottom);
        double p1 = map.invTransform(pos.x());
        double res = plot_->get_candle_resolution();
        p1 = res * static_cast<uint64_t>((p1+res/2.0)/res);
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

    QPolygon adjustedPoints(const QPolygon &points) const QWT_OVERRIDE
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

        if (!price_label_) return;

        // -------------------------------------------------
        // Right Y axis widget (price)
        //
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        QwtScaleWidget* yaw = plot_->axisWidget(QwtAxis::YRight);
        auto yawg = yaw->geometry();

        // Right Y axis scaling mapper
        const QwtScaleMap ymap = plot_->canvasMap(QwtAxis::YRight);
        auto y = ymap.transform(last_coord_.y());

        const QwtScaleDraw *ydraw = plot_->axisScaleDraw(QwtAxis::YRight);
        //
        // display price inside price axis
        //
        QwtText price_text = ydraw->label(last_coord_.y());
        QColor c("#555555");
        c.setAlpha(200);
        price_text.setColor(Qt::white);
        price_text.setBorderPen(QPen(c, 1));
        price_text.setBackgroundBrush(c);
        price_text.setLayoutAttribute(QwtText::LayoutAttribute::MinimumLayout, true);
        price_text.setRenderFlags(Qt::AlignLeft | Qt::AlignVCenter);

        // get size of text that will be drawn
        auto s = price_text.textSize();
        // position the label
        price_label_->setText(price_text);
        auto g = price_label_->geometry();

        g.moveTo(yawg.x() + ydraw->maxTickLength() + ydraw->spacing() - 1, yawg.y() + y - s.height() - 8/2);
        price_label_->setGeometry(g.x(), g.y(), s.width() + 4, s.height() + 8);

        // -------------------------------------------------
        // Bottom X axis widget (date)
        //
        QwtScaleWidget* xaw = plot_->axisWidget(QwtAxis::XBottom);
        auto xawg = xaw->geometry();

        // Bottom X axis scaling mapper
        const QwtScaleMap xmap = plot_->canvasMap(QwtAxis::XBottom);
        double px = quantize_x_coord(last_coord_.x());
        auto x = xmap.transform(px);

        const QwtScaleDraw *xdraw = plot_->axisScaleDraw(QwtAxis::XBottom);
        //
        // display date inside date axis
        //
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

        g.moveTo(xawg.x() + x - s.width()/2, xawg.y() + xdraw->maxTickLength() + xdraw->spacing() - 1);
        date_label_->setGeometry(g.x(), g.y(), s.width() + 4, s.height() + 8);

        //
        // display the stats of the candle under the cursor
        //
        plot_->display_candle_status(last_coord_.x());
    }

};
