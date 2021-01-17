#include <QPainter>
#include "OrderBookCurve.h"

OrderBookCurve::OrderBookCurve(const QString &title) : QwtPlotCurve (title)
{
    setRenderHint( QwtPlotItem::RenderAntialiased, true );

    // Don't display pattern items in the legend
    setItemAttribute(QwtPlotItem::Legend, false);

    //
    setCurveAttribute(QwtPlotCurve::Fitted, false);
}

void OrderBookCurve::drawLines (QPainter *p, const QwtScaleMap &xMap,
                         const QwtScaleMap &yMap, const QRectF &canvasRect, int from, int to) const
{

    const int numOfSegments = m_segColor.size();
    if (numOfSegments)
    {
        p->save();
        for(int i=0; i<numOfSegments; ++i)
        {
            p->setPen(m_segColor[i]);
            QwtPlotCurve::drawLines (p, xMap, yMap, canvasRect, m_segStart[i], m_segFinish[i]);
        }
        p->restore();
    }
    else
        QwtPlotCurve::drawLines (p, xMap, yMap, canvasRect, from, to);

}

void OrderBookCurve::setSegmentInfo(int segmentStartIndex, int segmentFinisIndex, const QColor & color)
{
    m_segColor.push_back(color);
    m_segStart.push_back(segmentStartIndex);
    m_segFinish.push_back(segmentFinisIndex);
}
