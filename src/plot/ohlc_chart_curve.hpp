#pragma once

// Qt
#include <QBrush>
#include <QPen>
#include <QVector>
// Qwt
#include <QwtPlotTradingCurve>
// Grox
#include "currency/ohlctv_sample.hpp"
#include "data/timebased_chart_data.hpp"

class ohlc_chart_curve : public QwtPlotTradingCurve
{
  public:
  enum GroxSymbolStyle : int
  {
    HeikinAshi = QwtPlotTradingCurve::SymbolStyle::UserSymbol + 1
  };

  public:
  using QwtPlotTradingCurve::QwtPlotTradingCurve;
  explicit ohlc_chart_curve(ohlc_chart_data* chartData);

  void drawSeries(QPainter*, QwtScaleMap const& xMap, QwtScaleMap const& yMap,
      QRectF const& canvasRect, int from, int to) const QWT_OVERRIDE;

  void drawSymbols(QPainter* painter, QwtScaleMap const& xMap, QwtScaleMap const& yMap,
      QRectF const& canvasRect, int from, int to) const QWT_OVERRIDE;

  void drawVolume(QPainter* painter, QwtScaleMap const& xMap, QwtScaleMap const& yMap,
      QRectF const& canvasRect, int from, int to) const;

  void drawVolumeBar(QPainter* painter, ohlctv_sample const& sample, double width) const;

  void setSymbolPenHA(Direction, QPen const&);
  void setSymbolBrushHA(Direction, QBrush const&);
  void setSymbolPenVolume(Direction, QPen const&);
  void setSymbolBrushVolume(Direction, QBrush const&);

  QPen HAPen[2];
  QBrush HABrush[2];
  QPen VolumePen[2];
  QBrush VolumeBrush[2];
  mutable bool start_heikin;
  mutable double prev_open;
  mutable double prev_close;
};
