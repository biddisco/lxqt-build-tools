#ifndef STOCKCHARTDATESCALEDDRAW_H
#define STOCKCHARTDATESCALEDDRAW_H

#include <qwt_date_scale_draw.h>

#include "PeriodValSegment.h"

class StockChartDateScaleDraw : public QwtDateScaleDraw
{
private:
    uint64_t min_val_;
    uint64_t max_val_;
    //
    QDateTime t1;
    QDateTime t2;

public:
    StockChartDateScaleDraw(Qt::TimeSpec timeSpec);
    //
    void set_min_max(uint64_t min, uint64_t max) {
        min_val_ = min;
        max_val_ = max;
        t1 = QDateTime::fromSecsSinceEpoch(static_cast<uint64_t>(min_val_));
        t2 = QDateTime::fromSecsSinceEpoch(static_cast<uint64_t>(max_val_));
        //
        auto days = t1.daysTo(t2);
        auto secs = t1.secsTo(t2);
        qint64 daysBetween = t2.date().toJulianDay() - t1.date().toJulianDay();
        QDate difference = QDate::fromJulianDay(daysBetween);
        QDate firstDate = QDate::fromJulianDay(0);
        int years = difference.year() - firstDate.year();
        int months = difference.month() - firstDate.month();
    }
    //
    virtual QwtText label( double ) const;
};

#endif // STOCKCHARTDATESCALEDDRAW_H
