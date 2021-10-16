#ifndef STOCKCHARTPLOTZOOMER_H
#define STOCKCHARTPLOTZOOMER_H

#include <qwt_plot_zoomer.h>

class QwtDateScaleDraw;

class StockChartPlotZoomer: public QwtPlotZoomer
{
private:
    const QwtDateScaleDraw *chartScale_;

public:
    StockChartPlotZoomer( QWidget *canvas );

    void setChartScale(const QwtDateScaleDraw *chartScale);

protected:
    virtual QwtText trackerTextF( const QPointF &pos ) const;
 };


#endif // STOCKCHARTPLOTZOOMER_H
