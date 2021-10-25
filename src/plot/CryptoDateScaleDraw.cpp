#include <cmath>
//
#include <QDebug>
//
#include <qwt_text.h>
//
#include <boost/date_time.hpp>
//
#include "src/util/QDateHelper.h"
#include "src/plot/CryptoDateScaleDraw.hpp"

// ----------------------------------------------------------------------------
CryptoDateScaleDraw::CryptoDateScaleDraw(Qt::TimeSpec timeSpec)
    : QwtDateScaleDraw(timeSpec)
{
    setDateFormat(QwtDate::Minute, "hh:mm");
    setDateFormat(QwtDate::Hour,   "hh:mm");
    setDateFormat(QwtDate::Day,    "dd");
    setDateFormat(QwtDate::Week,   "dd MMM");
    setDateFormat(QwtDate::Month,  "MMM");
    setDateFormat(QwtDate::Year,   "yyyy");
}

// ----------------------------------------------------------------------------
QwtText CryptoDateScaleDraw::label(double value) const
{
    const QDateTime dt = toDateTime(value);
    auto interval = intervalType(scaleDiv());
    if (interval == QwtDate::Hour && dt.time().hour()==0) {
        interval = QwtDate::IntervalType(int(interval)+1);
    }
    if (interval == QwtDate::Day && dt.date().day()==1) {
        return QLocale().toString(dt, "MMM");
    }
    if (interval == QwtDate::Month && dt.date().month()==1) {
        return QLocale().toString(dt, "yyyy");
    }
    const QString fmt = dateFormatOfDate(dt, interval);
    return QLocale().toString( dt, fmt );
}
