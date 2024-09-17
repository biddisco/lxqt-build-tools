// STL
#include <iostream>
#include <mutex>
#include <vector>
// Qt
#include <QList>
#include <QPainter>
#include <QPen>
#include <QString>
// Qwt
#include <QwtPlotCurve>
#include <QwtPlotItem>
#include <QwtScaleMap>
// Grox
#include "OrderBookCurve.h"

// ----------------------------------------------------------------------------
OrderBookCurve::OrderBookCurve(QString const& title)
  : QwtPlotCurve(title)
{
  setRenderHint(QwtPlotItem::RenderAntialiased, true);

  // Don't display pattern items in the legend
  setItemAttribute(QwtPlotItem::Legend, false);

  //
  setCurveAttribute(QwtPlotCurve::Fitted, false);
}

// ----------------------------------------------------------------------------
void OrderBookCurve::drawLines(QPainter* p, QwtScaleMap const& xMap, QwtScaleMap const& yMap,
  QRectF const& canvasRect, int from, int to) const
{
  std::unique_lock<std::mutex> lock(paint_mutex_, std::try_to_lock_t{});
  // if another thread is mdifying data, just exit without repainting
  if (!lock.owns_lock())
  {
    return;
  }

  const int numOfSegments = m_segPen.size();
  if (numOfSegments)
  {
    p->save();
    for (int i = 0; i < numOfSegments; ++i)
    {
      p->setPen(m_segPen[i]);
      QwtPlotCurve::drawLines(p, xMap, yMap, canvasRect, m_segStart[i], m_segFinish[i] - 1);
    }
    p->restore();
  }
  else
    QwtPlotCurve::drawLines(p, xMap, yMap, canvasRect, from, to);
}

// ----------------------------------------------------------------------------
void OrderBookCurve::setSegmentInfo(
  int segmentStartIndex, int segmentFinisIndex, QColor const& color, double thickness)
{
  // when we are changing data
  std::lock_guard<std::mutex> lock(paint_mutex_);
  //
  QPen pen(color);
  pen.setWidth(thickness);
  pen.setCosmetic(true);
  m_segPen.push_back(pen);
  m_segStart.push_back(segmentStartIndex);
  m_segFinish.push_back(segmentFinisIndex);
}

// ----------------------------------------------------------------------------
void OrderBookCurve::clear_samples()
{
  // when we are changing data
  std::lock_guard<std::mutex> lock(paint_mutex_);
  //
  QwtPlotCurve::setRawSamples(static_cast<float*>(nullptr), static_cast<float*>(nullptr), 0);
}

// ----------------------------------------------------------------------------
void OrderBookCurve::setRawSamples_locked(
  std::vector<float> const& xData, std::vector<float> const& yData)
{
  // prevent paint events when we are changing data
  std::lock_guard<std::mutex> lock(paint_mutex_);
  //
  if (xData.size() != yData.size())
  {
    std::cout << "FIX: rawdata sizes " << xData.size() << " " << yData.size() << std::endl;
  }
  assert(xData.size() == yData.size());
  //
  m_segStart.front() = 0;
  m_segFinish.front() = xData.size();
  QwtPlotCurve::setRawSamples(&xData[0], &yData[0], xData.size());
}
