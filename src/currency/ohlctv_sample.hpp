#pragma once

#include <iomanip>
#include <iostream>
//
#include <QwtOHLCSample>

using ohlctv_sample = QwtOHLCSample;

// ----------------------------------------------------------------------------
inline std::ostream& operator<<(std::ostream& os, ohlctv_sample const& x)
{
  os << "T: " << std::right << std::setw(13) << std::setprecision(14) << x.time << " "
     << "O: " << x.open << " "
     << "H: " << x.high << " "
     << "L: " << x.low << " "
     << "C: " << x.close << " "
     << "V: " << x.volume;
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
