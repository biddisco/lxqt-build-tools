#pragma once

#include <QwtOHLCSample>

using ohlctv_sample = QwtOHLCSample;

// ----------------------------------------------------------------------------
inline std::ostream& operator<<(std::ostream& os, ohlctv_sample const& x)
{
#if 0
    os << "Time: "   << x.time << " "
       << "Open: "   << x.open << " "
       << "High: "   << x.high << " "
       << "Low: "    << x.low << " "
       << "Close: "  << x.close << " "
       << "Volume: " << x.volume;
#else
  os << "T: " << x.time << " "
     << "O: " << x.open << " "
     << "H: " << x.high << " "
     << "L: " << x.low << " "
     << "C: " << x.close << " "
     << "V: " << x.volume;
#endif
  return os;
}

// ----------------------------------------------------------------------------
inline void update_ohlctv_sample(ohlctv_sample& ohlc, ohlctv_sample const& other)
{
  if (ohlc.isValid())
  {
    ohlc.low = std::min(ohlc.low, other.low);
    ohlc.high = std::max(ohlc.high, other.high);
    ohlc.close = other.close;
    ohlc.volume = ohlc.volume + other.volume;
  }
  else
  {
    ohlc = other;
  }
}

#if 0

/*!
   \brief Open-High-Low-Close sample used in financial charts

   In financial charts the movement of a price in a time interval is often
   represented by the opening/closing prices and the lowest/highest prices
   in this interval.

   \sa QwtTradingChartData
 */

class ohlctv_sample
{
  public:
  ohlctv_sample( double time = 0.0,
    double open = 0.0, double high = 0.0,
    double low = 0.0, double close = 0.0, double volume = 0.0 );

  //QwtInterval boundingInterval() const;

  bool isValid() const;

  /*!
       Time of the sample, usually a number representing
       a specific interval - like a day.
     */
  double time;

  //! Opening price
  double open;

  //! Highest price
  double high;

  //! Lowest price
  double low;

  //! Closing price
  double close;

  //! Traded volume
  double volume;
};

/*!
   Constructor

   \param t Time value
   \param o Open value
   \param h High value
   \param l Low value
   \param c Close value
 */
inline ohlctv_sample::ohlctv_sample(
  double t, double o, double h, double l, double c, double v )
  : time( t )
  , open( o )
  , high( h )
  , low( l )
  , close( c )
  , volume( v)
{
}

/*!
   \brief Check if a sample is valid

   A sample is valid, when all of the following checks are true:

   - low <= high
   - low <= open <= high
   - low <= close <= high

   \return True, when the sample is valid
 */
inline bool ohlctv_sample::isValid() const
{
  return ( low <= high )
    && ( open >= low )
    && ( open <= high )
    && ( close >= low )
    && ( close <= high );
}

/*!
   \brief Calculate the bounding interval of the OHLC values

   For valid samples the limits of this interval are always low/high.

   \return Bounding interval
   \sa isValid()
 */
//inline QwtInterval ohlctv_sample::boundingInterval() const
//{
//  double minY = open;
//  minY = qMin( minY, high );
//  minY = qMin( minY, low );
//  minY = qMin( minY, close );

//  double maxY = open;
//  maxY = qMax( maxY, high );
//  maxY = qMax( maxY, low );
//  maxY = qMax( maxY, close );

//  return QwtInterval( minY, maxY );
//}

#endif
