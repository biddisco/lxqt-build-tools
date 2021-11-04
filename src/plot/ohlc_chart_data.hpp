#pragma once

// Qwt
#include <QwtInterval>
#include <QwtSeriesData>
#include <QwtTradingChartData>
#include <QwtOHLCSample>

class ohlc_chart_data : public QwtTradingChartData
{
  public:
    static constexpr double minute   = 60.0*1000.0;
    static constexpr double minute3  = minute*3;
    static constexpr double minute5  = minute*5;
    static constexpr double minute15 = minute*15;
    static constexpr double minute30 = minute*30;
    static constexpr double hour     = minute*60;
    static constexpr double hour2    = hour*2;
    static constexpr double hour4    = hour*4;
    static constexpr double hour6    = hour*6;
    static constexpr double hour12   = hour*12;
    static constexpr double day      = hour*24;
    static constexpr double day3     = day*3;

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
