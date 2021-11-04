#pragma once

// Qt
#include <QDateTime>
#include <QPen>
#include <QPoint>
// Qwt
#include <QwtAxis>
#include <QwtPlot>
#include <QwtPlotPicker>
#include <QwtPickerMachine>
#include <QwtScaleMap>
#include <QwtText>

class CryptoPicker : public QwtPlotPicker
{
public:
    CryptoPicker(QWidget* canvas)
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

    virtual QwtText trackerTextF(const QPointF& pos) const QWT_OVERRIDE
    {
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(pos.x());
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
        if(points.size() == 1)
        {
            // get the pixel/plot coordinate transform
            auto axis = QwtAxis::XBottom;
            QwtPlot *plot_ = dynamic_cast<QwtPlot*>(canvas()->parentWidget());
            if(!plot_) return adjusted;
            const QwtScaleMap map = plot_->canvasMap(axis);

            // Map the x coord to the chart, snap it to a bin,
            // then invert the mapping
            double p1 = map.invTransform(points[0].x());
            p1 = 60000.0*static_cast<uint64_t>((p1+30000.0)/60000.0);
            p1 = map.transform(p1);
            QPoint p(p1, points[0].y());
            adjusted += p;
        }
        return adjusted;
    }

};
