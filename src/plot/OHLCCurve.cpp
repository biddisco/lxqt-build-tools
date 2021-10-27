#include <QPen>
//
#include <qwt_date.h>
//
#include "OHLCCurve.h"

OHLCCurve::OHLCCurve(const QString& title)
    : QwtPlotTradingCurve(title)
{
    //setTitle(title); // for the legend
    setItemAttribute(QwtPlotItem::Legend, false); // Legend not needed for price plot
    setOrientation( Qt::Vertical );

    // The stock chart data is mapped onto an integral scale, with the
    // first value being 0, the second value 1, and so on. This allows
    // values to be aligned across weekends, etc. We therefore size
    // the bars for this integral scale.
    setSymbolExtent(0.8 * 60.0*1000.0);
    setMinSymbolWidth(0.1);
    setMaxSymbolWidth(0.0);

    // Darkish green "#159f49", Darkish red "#df4249"
    setSymbolPen(QwtPlotTradingCurve::Increasing, QColor("#159f49"));
    setSymbolPen(QwtPlotTradingCurve::Decreasing, QColor("#df4249"));
    setSymbolBrush(QwtPlotTradingCurve::Increasing, QColor("#159f49"));
    setSymbolBrush(QwtPlotTradingCurve::Decreasing, QColor("#df4249"));
}

OHLCCurve::OHLCCurve(const QVector<QwtOHLCSample> &chartData)
    : OHLCCurve("Price")
{
    setSamples( chartData );
}
