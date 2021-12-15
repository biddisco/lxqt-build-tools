#pragma once
//
#include <iostream>
//
#include "src/data/ohlc_datasets.hpp"
#include "pipeline.hpp"
//
// Boost Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>
//
class trade_filter;

//----------------------------------------------------------------------------
// Exponentially decaying moving average
struct exponential_moving_average
{
//    exponential_moving_average(const exponential_moving_average &) = default;
//    exponential_moving_average &operator = (const exponential_moving_average &) = default;
    //
    exponential_moving_average(double decay_factor=1.0)
        : prev_time_(0)
        , decay_factor_(decay_factor)
        , xma_(0)
        , first_(true)
    {
        std::cout << "init EMA " << decay_factor << std::endl;
    }

    double operator() (const QwtOHLCSample &ohlc)
    {
        double seconds = ohlc.time/1000.0;
        double timePeriod = decay_factor_; // seconds
        if (!first_)
        {
            auto mult = 2.0 / (timePeriod + 1.0);
            xma_ = (ohlc.close - xma_) * mult + xma_;
        }
        else
        {
            xma_ = ohlc.close;
            prev_time_ = seconds;
            first_ = false;
        }
        return xma_;
    }

    double exponential_version(const QwtOHLCSample &ohlc)
    {
        double seconds = ohlc.time/1000.0;
        if (!first_)
        {
            double alpha = 1.0 - 1.0/exp(decay_factor_*(seconds-prev_time_));
            xma_ += alpha*(ohlc.close - xma_);
            prev_time_ = seconds;
        }
        else
        {
            xma_ = ohlc.close;
            prev_time_ = seconds;
            first_ = false;
        }
        return xma_;
    }

    inline double getLastResult() { return this->xma_; }

    pipeline::filter<double, const QwtOHLCSample &> f() { return *this; }

private:
    double  prev_time_;
    double  decay_factor_;
    double  xma_;
    bool    first_;
};

//----------------------------------------------------------------------------
class moving_average
{
public:
    moving_average(int N)
        : decay_acc_(boost::accumulators::tag::rolling_window::window_size = N)
        , ra_(0)
    {
        std::cout << "init moving_average" << std::endl;
    }

    double operator()(const double data)
    {
        decay_acc_(data);
        ra_ = boost::accumulators::rolling_mean(decay_acc_);
        return ra_;
    }

    inline double getLastResult() { return ra_; }

    pipeline::filter<double, const double> f() { return *this; }

private:
    boost::accumulators::accumulator_set<
        double,
        boost::accumulators::stats<boost::accumulators::tag::rolling_mean>
    > decay_acc_;
    //
    double ra_;
};

//----------------------------------------------------------------------------
// An input type that takes an OHLC value and provides a value function
// that returns the current value
struct ohlc_input
{
//    ohlc_input(const ohlc_input &) = default;
//    ohlc_input &operator = (const ohlc_input &) = default;
    //
    ohlc_input(const QwtOHLCSample &ohlc) : val_(ohlc) {};
    //
    void set(const QwtOHLCSample &newv) {
        uint64_t candle_old = static_cast<uint64_t>(val_.time / (60.0*1000));
        uint64_t candle_new = static_cast<uint64_t>(newv.time / (60.0*1000));
        if (candle_old == candle_new) {
            update_QwtOHLCSample(val_, newv);
            // val_.time = candle_old*(60*1000);
            val_.time = newv.time;
            std::cout << "Update existing candle " << msecs_unix_to_calendar_time(val_.time) << std::endl;
        }
        else {
            val_ = newv;
            // val_.time = candle_new*(60*1000);
            std::cout << "New candle started " << msecs_unix_to_calendar_time(val_.time) << std::endl;
        }
    }
    // returns the current OHLC candle being processed
    const QwtOHLCSample& operator()() { return val_; };

    pipeline::input<const QwtOHLCSample&> f() { return *this; }

private:
    QwtOHLCSample val_;
};

//----------------------------------------------------------------------------
struct ohlc_close
{
    // returns the close price of OHLC candle
    double operator()(const QwtOHLCSample &ohlc) const { return ohlc.close; };

    pipeline::filter<double, const QwtOHLCSample&> f() { return *this; }
};

//----------------------------------------------------------------------------
struct cross {
//    cross() = default;
//    cross(const cross &) = default;
//    cross &operator = (const cross &) = default;
    //
    bool operator() (double v1, double v2) {
        std::cout << v1 << " : " << v2 << std::endl;
        return v1>=v2;
    }
    pipeline::filter<bool, double, double> f() { return *this; }
};

//----------------------------------------------------------------------------
// Remove consecutive repetitions from a stream.
// used by cross detector to filter out change from true->false etc
template <typename T>
struct unique
{
    explicit unique()
        : first_(false)
        , prev_() {}

    std::optional<T> operator()(T value)
    {
        if (first_ || (value != prev_))
        {
            first_ = false;
            return std::optional<T>(prev_ = value);
        }
        return std::optional<T>{};
    }

    pipeline::filter<std::optional<T>, T> f() { return *this; }

private:
    bool first_;
    T    prev_;
};

//----------------------------------------------------------------------------
//
// class to hold our stream based data
//
class trade_filter {
public:
    trade_filter();
    virtual ~trade_filter() {}
    //
    void process(const QwtOHLCSample &ohlc);
    //
    moving_average rolling_average_;
    //
    ohlc_input ohlc_in_;
    pipeline::input<const QwtOHLCSample&> sample_input_;
    //
    exponential_moving_average ema_1;
    exponential_moving_average ema_10;
    //
    pipeline::input<void> cross_detector_;
};
