#include "StockChartDateScaleDraw.h"
#include <QDebug>
#include <math.h>
#include "QDateHelper.h"
#include <qwt_text.h>

StockChartDateScaleDraw::StockChartDateScaleDraw(Qt::TimeSpec /*timeSpec*/)
{
    setDateFormat( QwtDate::Minute, "hh:mm" );
    setDateFormat( QwtDate::Hour, "hh:mm" );
    setDateFormat( QwtDate::Day, "ddd dd MMM" );
    setDateFormat( QwtDate::Week, "Www" );
    setDateFormat( QwtDate::Month, "MMM" );

//    setDateFormat( QwtDate::Millisecond, "hh:mm:ss:zzz\nddd dd MMM" );
//    setDateFormat( QwtDate::Second, "hh:mm:ss\nddd dd MMM" );
//    setDateFormat( QwtDate::Minute, "hh:mm\nddd dd MMM" );
//    setDateFormat( QwtDate::Hour, "hh:mm\nddd dd MMM" );
//    setDateFormat( QwtDate::Day, "ddd dd MMM" );
//    setDateFormat( QwtDate::Week, "Www" );
//    setDateFormat( QwtDate::Month, "MMM" );
}

QwtText StockChartDateScaleDraw::label( double labelVal ) const
{
    using namespace boost::posix_time;

    // Use floor() here if seconds are always positive.
    time_t secondsSinceEpoch = floor(labelVal);
    boost::posix_time::ptime result = boost::posix_time::from_time_t(secondsSinceEpoch);

    QString labelTime = QDateHelper::boostToQDateTime(result).toString();
    return QwtText(labelTime);

}
