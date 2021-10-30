#pragma once

// STL
#include <mutex>
#include <vector>
// Qt
#include <QList>
#include <QString>
#include <QPainter>
#include <QPen>
// Qwt
#include <QwtScaleMap>
#include <QwtPlotCurve>

// ----------------------------------------------------------------------------
class OrderBookCurve : public QwtPlotCurve
{
public:
    explicit OrderBookCurve(const QString &title = QString());

    virtual void  drawLines (QPainter *p, const QwtScaleMap &xMap,
                             const QwtScaleMap &yMap, const QRectF &canvasRect, int from, int to) const override;

    void setSegmentInfo(int segmentStartIndex, int segmentFinisIndex, const QColor & color, double thickness);

    void clear_samples();
    void setRawSamples_locked(
        std::vector<float> const &xData, std::vector<float> const &yData);

private:
    QList<QPen>     m_segPen;
    QList<int>      m_segStart;
    QList<int>      m_segFinish;
    //
    mutable std::mutex paint_mutex_;
 };
