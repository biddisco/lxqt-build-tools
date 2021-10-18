#ifndef _PLOT_H_
#define _PLOT_H_

#include <QDateTime>
//
#include <qwt_plot.h>
#include <qwt_samples.h>

#include "StockChartPlotZoomer.h"
//#include "InstrumentSelectionInfo.h"
#include "StockChartDateScaleDraw.h"

//class TimeScaleDraw: public QwtScaleDraw
//{
//    uint64_t min_val_;
//    uint64_t max_val_;
//    //
//    QDateTime t1;
//    QDateTime t2;
//    //
//    void set_min_max(uint64_t min, uint64_t max) {
//        min_val_ = min;
//        max_val_ = max;
//        t1 = QDateTime::fromSecsSinceEpoch(static_cast<uint64_t>(min_val_));
//        t2 = QDateTime::fromSecsSinceEpoch(static_cast<uint64_t>(max_val_));
//        //
//        auto days = t1.daysTo(t2);
//        auto secs = t1.secsTo(t2);
//        qint64 daysBetween = t2.date().toJulianDay() - t1.date().toJulianDay();
//        QDate difference = QDate::fromJulianDay(daysBetween);
//        QDate firstDate = QDate::fromJulianDay(0);
//        int years = difference.year() - firstDate.year();
//        int months = difference.month() - firstDate.month();

//    }

//    virtual QwtText label(double v) const
//    {
//        QDateTime t = QDateTime::fromSecsSinceEpoch(static_cast<uint64_t>(v));
//        return t.toString("hh:mm");
//    }
//};

class PriceAndPatternPlot: public QwtPlot
{
    Q_OBJECT

private:
    StockChartPlotZoomer    *plotZoomer_;
    QwtDateScaleDraw/*StockChartDateScaleDraw*/ *timescaleDraw_;


public:
    PriceAndPatternPlot( QWidget * = NULL );
//    void populateChartData(const InstrumentSelectionInfoPtr &instrSelInfo);
    void set_OHLC_data(const QVector<QwtOHLCSample> &ohlc);

    void clearPatternPlots();
    void setupWheelZooming();

public Q_SLOTS:
    void setMode( int );
    void exportPlot();

private Q_SLOTS:
    void showItem( QwtPlotItem *, bool on );
};

#endif
