#include <QPen>
//
#include <qwt_date.h>
//
#include "OHLCCurve.h"

OHLCCurve::OHLCCurve(const QVector<QwtOHLCSample> &chartData)
{
    setSamples( chartData );
    setTitle( "Price" ); // for the legend
    setItemAttribute(QwtPlotItem::Legend, false); // Legend not needed for price plot
    setOrientation( Qt::Vertical );

    // The stock chart data is mapped onto an integral scale, with the
    // first value being 0, the second value 1, and so on. This allows
    // values to be aligned across weekends, etc. We therefore size
    // the bars for this integral scale.
    setSymbolExtent( 0.8 );
    setMinSymbolWidth( 3 );
    setMaxSymbolWidth( 0.0 );

    setSymbolPen(QwtPlotTradingCurve::Increasing, Qt::green );
    setSymbolPen(QwtPlotTradingCurve::Decreasing, Qt::red );
    setSymbolBrush(QwtPlotTradingCurve::Increasing, Qt::green );
    setSymbolBrush(QwtPlotTradingCurve::Decreasing, Qt::red );

}
