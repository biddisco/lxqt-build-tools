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
#include <QwtScaleMap>
#include <QwtText>
//
#include "src/plot/ohlc_price_plot.hpp"

class ohlc_picker : public QwtPlotPicker
{
public:
    ohlc_picker(QWidget* canvas)
        : QwtPlotPicker(canvas)
    {
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
        //            QColor c = rubberBandPen().color();
        //            text.setBorderPen(QPen(c));
        //            text.setBorderRadius(6);
        //            c.setAlpha(170);
        //text.setBackgroundBrush(c);
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

};
