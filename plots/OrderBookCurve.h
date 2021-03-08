#pragma once

#include <QList>
#include <qwt_plot_curve.h>

class OrderBookCurve : public QwtPlotCurve
{
public:
    explicit OrderBookCurve(const QString &title = QString());

    virtual void  drawLines (QPainter *p, const QwtScaleMap &xMap,
                             const QwtScaleMap &yMap, const QRectF &canvasRect, int from, int to) const;

    void setSegmentInfo(int segmentStartIndex, int segmentFinisIndex, const QColor & color, double thickness);

private:
    QList<QPen>     m_segPen;
    QList<int>      m_segStart;
    QList<int>      m_segFinish;
 };
