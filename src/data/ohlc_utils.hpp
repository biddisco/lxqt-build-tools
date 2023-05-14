#pragma once

// STL
#include <optional>
#include <vector>
// Qwt
#include <QwtOHLCSample>

void update_QwtOHLCSample(QwtOHLCSample& ohlc, QwtOHLCSample const& other);

struct ohlc_resample
{
  QwtOHLCSample ohlc_;
  //
  ohlc_resample(const QwtOHLCSample& ohlc)
    : ohlc_(ohlc)
  {
  }
  //
  QwtOHLCSample operator()(QwtOHLCSample const& other)
  {
    update_QwtOHLCSample(ohlc_, other);
    return ohlc_;
  }
};

// ----------------------------------------------------------------------------
struct ohlcv_minmax
{
  double min_price_;
  double max_price_;
  double min_volume_;
  double max_volume_;
  bool valid_;

  bool isValid() const
  {
    return valid_;
  }

  ohlcv_minmax unite(const ohlcv_minmax& other) const
  {
    if (!isValid())
    {
      if (!other.isValid())
        return ohlcv_minmax();
      else
        return other;
    }

    if (!other.isValid())
      return *this;

    ohlcv_minmax united;
    united.min_price_ = std::min(min_price_, other.min_price_);
    united.max_price_ = std::max(max_price_, other.max_price_);
    united.min_volume_ = std::min(min_volume_, other.min_volume_);
    united.max_volume_ = std::max(max_volume_, other.max_volume_);
    return united;
  }
};

// ----------------------------------------------------------------------------
struct ohlc_candlemaker
{
  double to_resolution_;
  double from_resolution_;
  QwtOHLCSample ohlc_;
  //
  ohlc_candlemaker(double to_resolution, double from_resolution)
    : to_resolution_(to_resolution)
    , from_resolution_(from_resolution)
    , ohlc_()
  {
  }
  //
  std::optional<QwtOHLCSample> operator()(const QwtOHLCSample& ohlc)
  {
    uint64_t candle_old = static_cast<uint64_t>(ohlc_.time / to_resolution_);
    uint64_t candle_cur = static_cast<uint64_t>(ohlc.time / to_resolution_);
    if (candle_old == candle_cur)
    {
      update_QwtOHLCSample(ohlc_, ohlc);
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
  QwtOHLCSample val_;
};
