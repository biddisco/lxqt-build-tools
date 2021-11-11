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
#include <QwtText>
//
#include "src/plot/ohlc_price_plot.hpp"

class ohlc_picker : public QwtPlotPicker
{
public:
    mutable QPointF last_coord;
    QwtTextLabel  *price_label;

    ohlc_picker(QWidget* canvas)
        : QwtPlotPicker(canvas)
        , last_coord(0,0)
        , price_label(nullptr)
    {
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas->parentWidget());
        QwtScaleWidget* aw = plot_->axisWidget(QwtAxis::YRight);
        price_label = new QwtTextLabel(canvas->parentWidget());

        setTrackerMode(QwtPlotPicker::ActiveOnly);
        setRubberBand(
                    QwtPicker::RubberBand(int(QwtPicker::HLineRubberBand) + int(QwtPicker::VLineRubberBand)));
        setStateMachine(new QwtPickerTrackerMachine());
        // pale blue "#8589cf"
        setRubberBandPen(QPen(QBrush("#8589cf"), 1, Qt::DashLine));
        setTrackerPen(QPen(Qt::darkGray));

    }

    QPointF quantize_x_coord(const QPointF& pos) const
    {
        // get the pixel/plot coordinate transform
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        if (!plot_) return pos;
        //
        const QwtScaleMap map = plot_->canvasMap(QwtAxis::XBottom);
        double p1 = map.invTransform(pos.x());
        double res = plot_->get_candle_resolution();
        p1 = res*static_cast<uint64_t>((p1+res/2.0)/res);
        p1 = map.transform(p1);
        return QPointF(p1, pos.y());
    }

    virtual QwtText trackerTextF(const QPointF& pos) const QWT_OVERRIDE
    {
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        if (!plot_) return QwtText();
        //
        double res = plot_->get_candle_resolution();
        double p1 = res*static_cast<uint64_t>((pos.x()+res/2.0)/res);
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(p1);
        QString s = QLocale().toString(dt, "dd-MM-yy hh:mm");
        QwtText text(s);
        text.setColor(Qt::lightGray);
        //
        last_coord = pos;
        return text;
    }

    QPolygon adjustedPoints(const QPolygon &points) const QWT_OVERRIDE
    {
        QPolygon adjusted;
        // we only handle hLine so far
        if (points.size() == 1)
        {
            // Map the x coord to the chart, snap it to a bin,
            // then invert the mapping
            adjusted += QPoint(quantize_x_coord(points[0]).x(), points[0].y());
        }
        return adjusted;
    }

    virtual void updateDisplay() QWT_OVERRIDE
    {
        QwtPlotPicker::updateDisplay();

        if (!price_label) return;

        // axis widget
        ohlc_price_plot *plot_ = dynamic_cast<ohlc_price_plot*>(canvas()->parentWidget());
        QwtScaleWidget* aw = plot_->axisWidget(QwtAxis::YRight);
        auto awg = aw->geometry();
        // scaling mapper
        const QwtScaleMap map = plot_->canvasMap(QwtAxis::YRight);
        auto y = map.transform(last_coord.y());
        // setup string
        QString str = QString::number(last_coord.y(), 'g', 4);
        QwtText trackerText(str);
        QColor c("#555555");
        c.setAlpha(200);
        trackerText.setColor(Qt::white);
        trackerText.setBorderPen(QPen(c, 1));
        trackerText.setBackgroundBrush(c);
        trackerText.setLayoutAttribute(QwtText::LayoutAttribute::MinimumLayout, true);
        trackerText.setRenderFlags(Qt::AlignLeft| Qt::AlignVCenter);
        // get size of text that will be drawn
        auto s = trackerText.textSize();
        // position the label
        price_label->setText(trackerText);
        auto g = price_label->geometry();

        g.moveTo(awg.x() + 11, awg.y() + y - s.height());
        price_label->setGeometry(g.x(), g.y(),
                                 awg.width() - 12, s.height()+2);

    }

};
