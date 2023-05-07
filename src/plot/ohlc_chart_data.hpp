#pragma once

// STL
#include <vector>
// Qwt
#include <QwtInterval>
#include <QwtSeriesData>
#include <QwtTradingChartData>
//
#include "data/ohlc_data_resolutions.hpp"

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
