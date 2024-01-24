#pragma once

// STL
#include <iostream>
#include <optional>
#include <vector>
// extern
#include <magic_enum.hpp>
// grox
#include "data/ohlctv_sample.hpp"

void update_ohlctv_sample(ohlctv_sample& ohlc, ohlctv_sample const& other);

enum class ohlc_modes : int
{
  open = 1,
  close = 2,
  mid_open_close = 3,
  high = 4,
  low = 5,
  mid_high_low = 6,
  volume = 7,
  value = 8
};

constexpr auto ohlc_mode_names = magic_enum::enum_names<ohlc_modes>();

inline double ohlc_mode_extract(const ohlc_modes mode, ohlctv_sample const& ohlc)
{
  switch (mode)
  {
  case ohlc_modes::open:
    return ohlc.open;
  case ohlc_modes::close:
    return ohlc.close;
  case ohlc_modes::mid_open_close:
    return 0.5 * (ohlc.open + ohlc.close);
  case ohlc_modes::high:
    return ohlc.high;
  case ohlc_modes::low:
    return ohlc.low;
  case ohlc_modes::mid_high_low:
    return 0.5 * (ohlc.low + ohlc.high);
  case ohlc_modes::volume:
    return ohlc.volume;
  case ohlc_modes::value:
    return ohlc.volume * (0.5 * (ohlc.open + ohlc.close));
  default:
    return 0.0;
  }
}

inline std::ostream& operator<<(std::ostream& os, const ohlc_modes& m)
{
  os << int(m);
  return os;
}

inline double get_time(ohlctv_sample const& val)
{
  return val.time;
}
inline double get_time(QPointF const& val)
{
  return val.x();
}

struct ohlc_resample
{
  ohlctv_sample ohlc_;
  //
  ohlc_resample(ohlctv_sample const& ohlc)
    : ohlc_(ohlc)
  {
  }
  //
  ohlctv_sample operator()(ohlctv_sample const& other)
  {
    update_ohlctv_sample(ohlc_, other);
    return ohlc_;
  }
};

template <typename DataType>
struct minmax_data;

template <>
struct minmax_data<QPointF>
{
  double min_;
  double max_;
  bool valid_;

  minmax_data()
    : valid_{false}
  {
  }

  minmax_data<QPointF>& update(minmax_data<QPointF> const& other)
  {
    if (!isValid())
    {
      if (other.isValid())
        *this = other;
      return *this;
    }

    if (!other.isValid())
      return *this;

    min_ = std::min(min_, other.min_);
    max_ = std::min(max_, other.max_);
    return *this;
  }

  bool isValid() const
  {
    return valid_;
  }
};

template <>
struct minmax_data<ohlctv_sample>
{
  double min_price_;
  double max_price_;
  double min_volume_;
  double max_volume_;
  bool valid_;

  minmax_data()
    : min_price_{0}
    , max_price_{0}
    , min_volume_{0}
    , max_volume_{0}
    , valid_{false}
  {
  }

  minmax_data(ohlctv_sample const& init)
    : min_price_{init.low}
    , max_price_{init.high}
    , min_volume_{init.volume}
    , max_volume_{init.volume}
    , valid_(true)
  {
  }

  bool isValid() const
  {
    return valid_;
  }

  minmax_data<ohlctv_sample>& update(minmax_data<ohlctv_sample> const& other)
  {
    if (!isValid())
    {
      if (other.isValid())
        *this = other;
      return *this;
    }

    if (!other.isValid())
      return *this;

    min_price_ = std::min(min_price_, other.min_price_);
    max_price_ = std::max(max_price_, other.max_price_);
    min_volume_ = std::min(min_volume_, other.min_volume_);
    max_volume_ = std::max(max_volume_, other.max_volume_);
    return *this;
  }
};

// ----------------------------------------------------------------------------
using ohlcv_minmax = minmax_data<ohlctv_sample>;

// ----------------------------------------------------------------------------
struct ohlc_candlemaker
{
  double to_resolution_;
  double from_resolution_;
  ohlctv_sample ohlc_;
  //
  ohlc_candlemaker(double to_resolution, double from_resolution)
    : to_resolution_(to_resolution)
    , from_resolution_(from_resolution)
    , ohlc_()
  {
  }
  //
  std::optional<ohlctv_sample> operator()(ohlctv_sample const& ohlc)
  {
    uint64_t candle_old = static_cast<uint64_t>(ohlc_.time / to_resolution_);
    uint64_t candle_cur = static_cast<uint64_t>(ohlc.time / to_resolution_);
    if (candle_old == candle_cur)
    {
      update_ohlctv_sample(ohlc_, ohlc);
    }
    else
    {
      ohlc_ = ohlc;
      ohlc_.time = candle_cur * to_resolution_;
    }
    // is this the last candle before we start a new one
    uint64_t candle_next = static_cast<uint64_t>((ohlc.time + from_resolution_) / to_resolution_);
    if (candle_next > candle_cur)
    {
      return ohlc_;
    }
    return std::nullopt;
  }

  private:
  ohlctv_sample val_;
};
