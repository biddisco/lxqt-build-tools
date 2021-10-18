#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <qwt_date.h>


namespace QDateHelper
{
    QDateTime boostToQDateTime(const boost::posix_time::ptime &boostTime);
}
