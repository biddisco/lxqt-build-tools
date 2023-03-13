#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtSeriesData>
#include <QwtTradingChartData>

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
struct candle_res {
    // this is the actual resolution of the candle
    double res_;
    // this is used when resampling to know which (higher) res to use
    double base_;
    // A simple name that will appear in menus
    char const *name_;
    // operators to make access easy
    constexpr operator double() const { return res_; }
    constexpr operator const char*() const { return name_; }
    bool operator <  (const candle_res &other) { return res_ <  other.res_; }
    bool operator >  (const candle_res &other) { return res_ >  other.res_; }
    bool operator == (const candle_res &other) { return res_ == other.res_; }
};

// ----------------------------------------------------------------------------
struct ohlcv_minmax {
    double min_price_;
    double max_price_;
    double min_volume_;
    double max_volume_;
    bool   valid_;

    bool isValid() const { return valid_; }

    ohlcv_minmax unite( const ohlcv_minmax &other ) const
    {
        if (!isValid()) {
            if (!other.isValid())
                return ohlcv_minmax();
            else
                return other;
        }

        if ( !other.isValid() )
            return *this;

        ohlcv_minmax united;
        united.min_price_  = std::min(min_price_, other.min_price_);
        united.max_price_  = std::max(max_price_, other.max_price_);
        united.min_volume_ = std::min(min_volume_, other.min_volume_);
        united.max_volume_ = std::max(max_volume_, other.max_volume_);
        return united;
    }
};

// ----------------------------------------------------------------------------
class ohlc_chart_data : public QwtTradingChartData
{
  public:
    static constexpr candle_res minute   = {60*1000,   1,        "1m"  };
    static constexpr candle_res minute3  = {minute*3,  minute,   "3m"  };
    static constexpr candle_res minute5  = {minute*5,  minute,   "5m"  };
    static constexpr candle_res minute10 = {minute*10, minute5,  "10m" };
    static constexpr candle_res minute15 = {minute*15, minute5,  "15m" };
    static constexpr candle_res minute30 = {minute*30, minute15, "30m" };
    static constexpr candle_res hour     = {minute*60, minute30, "1h"  };
    static constexpr candle_res hour2    = {hour*2,    hour,     "2h"  };
    static constexpr candle_res hour4    = {hour*4,    hour2,    "4h"  };
    static constexpr candle_res hour6    = {hour*6,    hour2,    "6h"  };
    static constexpr candle_res hour12   = {hour*12,   hour6,    "12h" };
    static constexpr candle_res day      = {hour*24,   hour12,   "1d"  };
    static constexpr candle_res day2     = {day*2,     day,      "2d"  };
    static constexpr candle_res day3     = {day*3,     day,      "3d"  };
    static constexpr candle_res day7     = {day*7,     day,      "7d"  };
    static constexpr candle_res day15    = {day*15,    day3,     "15d" };

    // for easy access to array of all available resolutions
    static const std::vector<candle_res> &available_resolutions()
    {
        static const std::vector<candle_res> resolutions = {
            minute, minute3, minute5, minute10, minute15, minute30,
            hour, hour2, hour4, hour6, hour12, day, day2, day3, day7, day15
        };
        return resolutions;
    }

    static candle_res get_resolution(double res)
    {
        for (const auto &r : available_resolutions()) {
            if (r.res_ == res)
                return r;
        }
        throw std::runtime_error("Resolution not found");
    }

    // return best resolution -
    // gcd(min3, min5)  = min1
    // gcd(min5, min15) = min5
    // gcd(hour4, hour6) = hour2
    static candle_res gcd(candle_res a, candle_res b)
    {
        if (a>b) std::swap(a,b);
        while (a.res_ > ohlc_chart_data::minute) {
            while (b.res_ >= a.res_) {
                if (b.res_ == a.res_) return b;
                b = get_resolution(b.base_);
            }
            a = get_resolution(a.base_);
        }
        return ohlc_chart_data::minute;
    }

  protected:
    double  resolution_;

  public:
    ohlc_chart_data(double resolution)
        : QwtTradingChartData()
        , resolution_(resolution)
    {}

    ~ohlc_chart_data() {}

    ohlcv_minmax minmax_limits(size_t from, size_t to) const
    {
        auto const &init = m_samples[from];
        ohlcv_minmax result{init.low, init.high, init.volume, init.volume, true};
        for (size_t i=from; i<=to; ++i) {
            auto const &ohlc = m_samples[i];
            result.min_price_  = std::min(result.min_price_, ohlc.low);
            result.max_price_  = std::max(result.max_price_, ohlc.high);
            result.min_volume_ = std::min(result.min_volume_, ohlc.volume);
            result.max_volume_ = std::max(result.max_volume_, ohlc.volume);
        }
        return result;
    }

    inline void append( const QwtOHLCSample& data )
    {
        m_samples += data;
    }

    void clear()
    {
        m_samples.clear();
        m_samples.squeeze();
        cachedBoundingRect = QRectF( 0.0, 0.0, -1.0, -1.0 );
    }

    inline double get_resolution() const { return resolution_; }

    // return the index of the sample at time t
    inline int64_t sample_index(double time) const
    {
        int64_t i = static_cast<int64_t>((time-m_samples[0].time)/resolution_);
        return std::max(int64_t(0), i);
    }

    // return the index of the sample at time t
    inline double sample_time(int64_t i) const
    {
        double t = (i*resolution_) + m_samples[0].time;
        return t;
    }

    inline QVector<QwtOHLCSample> const &data() const { return m_samples; }
    inline QVector<QwtOHLCSample> &data() { return m_samples; };
};
