#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtSeriesData>
#include <QwtTradingChartData>
#include <QwtOHLCSample>

struct candle_res {
    // this is the actual resolution of the candle
    const double res_;
    // this is used when resampling to know which res to use
    const double base_;
    // A simple name that will appear in menus
    const char *name_;
    // operators to make access easy
    constexpr operator double() const { return res_; }
    constexpr operator const char*() const { return name_; }
};

class ohlc_chart_data : public QwtTradingChartData
{
  public:
    static constexpr candle_res minute   = {60*1000,   1,        "1m" };
    static constexpr candle_res minute3  = {minute*3,  minute,   "3m" };
    static constexpr candle_res minute5  = {minute*5,  minute,   "5m" };
    static constexpr candle_res minute15 = {minute*15, minute5,  "15m"};
    static constexpr candle_res minute30 = {minute*30, minute15, "30m"};
    static constexpr candle_res hour     = {minute*60, minute30, "1h" };
    static constexpr candle_res hour2    = {hour*2,    hour,     "2h" };
    static constexpr candle_res hour4    = {hour*4,    hour2,    "4h" };
    static constexpr candle_res hour6    = {hour*6,    hour2,    "6h" };
    static constexpr candle_res hour12   = {hour*12,   hour6,    "12h"};
    static constexpr candle_res day      = {hour*24,   hour12,   "1d" };
    static constexpr candle_res day3     = {day*3,     day,      "3d" };

    // for easy access to array of all available resolutions
    static const std::vector<candle_res> &available_resolutions()
    {
        static const std::vector<candle_res> resolutions = {
            minute, minute3, minute5, minute15, minute30,
            hour, hour2, hour4, hour6, hour12, day, day3
        };
        return resolutions;
    }

  public:
    ohlc_chart_data() : QwtTradingChartData() {}
    ~ohlc_chart_data() {}

    QwtInterval minmax_limits(size_t from, size_t to) const
    {
        auto const &init = m_samples[from];
        QwtInterval result(init.low, init.high);
        for (size_t i=from; i<=to; ++i) {
            auto const &ohlc = m_samples[i];
            result |= ohlc.low;
            result |= ohlc.high;
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

    QVector<QwtOHLCSample> const &data() const { return m_samples; }
    QVector<QwtOHLCSample> &data() { return m_samples; };
};
