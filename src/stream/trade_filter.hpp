#pragma once
//
#include <iostream>
#include <optional>
//
#include "data/ohlc_datasets.hpp"
#include "data/ohlc_heikin_ashi.hpp"
#include "pipeline.hpp"
//
// Boost Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>

//----------------------------------------------------------------------------
struct volume_weighted_moving_average
{
  // mode : 0=open, 1=close, 2=mid(open,close), 3=high, 4=low, 5=mid(high,low)
  volume_weighted_moving_average(int N, int mode = 2)
    : decay_acc_(boost::accumulators::tag::rolling_window::window_size = N)
    , ra_(0)
    , mode_(mode)
  {
  }

  double operator()(const QwtOHLCSample& val)
  {
    double price;
    if (mode_ == 2)
    {
      price = 0.5 * (val.open + val.close);
    }
    // insert data into boost accumulator
    decay_acc_(price, boost::accumulators::weight = val.volume);
    ra_ = boost::accumulators::rolling_mean(decay_acc_);
    return ra_;
  }

  inline double getLastResult()
  {
    return ra_;
  }

  pipeline::filter<double, const QwtOHLCSample&> f()
  {
    return *this;
  }

  private:
  boost::accumulators::accumulator_set<double,    // price
    boost::accumulators::stats<boost::accumulators::tag::rolling_mean>,
    double    // weight (volume)
    >
    decay_acc_;
  //
  double ra_;
  int mode_;
};

//----------------------------------------------------------------------------
struct moving_average
{
  // mode : 0=open, 1=close, 2=mid(open,close), 3=high, 4=low, 5=mid(high,low)
  moving_average(int N, int mode = 2)
    : decay_acc_(boost::accumulators::tag::rolling_window::window_size = N)
    , ra_(0)
  {
  }

  double operator()(double val)
  {
    // insert data into boost accumulator
    decay_acc_(val);
    ra_ = boost::accumulators::rolling_mean(decay_acc_);
    return ra_;
  }

  inline double getLastResult()
  {
    return ra_;
  }

  pipeline::filter<double, double> f()
  {
    return *this;
  }

  private:
  boost::accumulators::accumulator_set<double,    // price
    boost::accumulators::stats<boost::accumulators::tag::rolling_mean>>
    decay_acc_;
  //
  double ra_;
};

//----------------------------------------------------------------------------
struct gradient_change
{
  gradient_change()
    : last_(0.0)
    , first_(true)
  {
  }

  buy_sell_type operator()(double val)
  {
    const double epsilon = 0.002;
    buy_sell_type e = buy_sell_type::no_event;
    if (first_)
    {
      first_ = false;
      last_ = val;
    }
    else if (val > (last_ + epsilon))
    {
      e = buy_sell_type::buy_event;
      last_ = val;
    }
    else if (val < (last_ - epsilon))
    {
      e = buy_sell_type::sell_event;
      last_ = val;
    }
    return e;
  }

  pipeline::filter<buy_sell_type, double> f()
  {
    return *this;
  }

  private:
  //
  double last_;
  bool first_;
};

//----------------------------------------------------------------------------
// Exponentially decaying moving average
struct exponential_moving_average
{
  //    exponential_moving_average(const exponential_moving_average &) = default;
  //    exponential_moving_average &operator = (const exponential_moving_average &) = default;
  //
  exponential_moving_average(double decay_factor = 1.0)
    : prev_time_(0)
    , decay_factor_(decay_factor)
    , xma_(0)
    , first_(true)
  {
    std::cout << "init EMA " << decay_factor << std::endl;
  }

  double compute(const QwtOHLCSample& ohlc, double alpha)
  {
    double seconds = ohlc.time / 1000.0;
    if (!first_)
    {
      xma_ = (alpha * xma_) + (1.0 - alpha) * ohlc.close;
      std::cout << "EMA decay " << decay_factor_ << ", alpha " << alpha << " : " << xma_ << "\n";
    }
    else
    {
      xma_ = ohlc.close;
      first_ = false;
    }
    prev_time_ = seconds;
    return xma_;
  }

  double simple_version(const QwtOHLCSample& ohlc)
  {
    double alpha = 2.0 / (decay_factor_ + 1.0);
    return compute(ohlc, alpha);
  }

  double exponential_version(const QwtOHLCSample& ohlc)
  {
    if (first_)
      return compute(ohlc, 1.0);
    //
    double seconds = ohlc.time / 1000.0;
    double dt = (seconds - prev_time_);
    double alpha = 1.0 / exp(dt / decay_factor_);
    return compute(ohlc, alpha);
  }

  double operator()(const QwtOHLCSample& ohlc)
  {
    return exponential_version(ohlc);
  }

  inline double getLastResult()
  {
    return this->xma_;
  }

  pipeline::filter<double, const QwtOHLCSample&> f()
  {
    return *this;
  }

  private:
  double prev_time_;
  double decay_factor_;
  double xma_;
  bool first_;
};

//----------------------------------------------------------------------------
// An input type that takes an OHLC value and updates/accumulates its internal
// candle provides to provide a current candle value
// this is intended to be used with incoming trades to build live candles
struct ohlc_input
{
  ohlc_input(const QwtOHLCSample& ohlc)
    : val_(ohlc){};
  //
  void set(const QwtOHLCSample& newv)
  {
    uint64_t candle_old = static_cast<uint64_t>(val_.time / (60.0 * 1000));
    uint64_t candle_new = static_cast<uint64_t>(newv.time / (60.0 * 1000));
    if (candle_old == candle_new)
    {
      update_QwtOHLCSample(val_, newv);
      // val_.time = candle_old*(60*1000);
      val_.time = newv.time;
      std::cout << "Update existing candle " << msecs_unix_to_calendar_time(val_.time) << std::endl;
    }
    else
    {
      val_ = newv;
      // val_.time = candle_new*(60*1000);
      std::cout << "New candle started " << msecs_unix_to_calendar_time(val_.time) << std::endl;
    }
  }
  // returns the current OHLC candle being processed
  const QwtOHLCSample& operator()()
  {
    return val_;
  };

  pipeline::input<const QwtOHLCSample&> f()
  {
    return *this;
  }

  private:
  QwtOHLCSample val_;
};

//----------------------------------------------------------------------------
// an input object that just provides the latest value
template <typename T>
struct input_value
{
  input_value(const T& val)
    : val_(val){};
  //
  void set(const T& newv)
  {
    val_ = newv;
  }
  // returns the current OHLC candle being processed
  const T& operator()()
  {
    return val_;
  };

  pipeline::input<const T&> f()
  {
    return *this;
  }

  private:
  T val_;
};

//----------------------------------------------------------------------------
// an input object that just provides the latest value
template <typename T>
struct dummy
{
  dummy(){};
  //
  // returns the current OHLC candle being processed
  T operator()(T val)
  {
    return val;
  };

  pipeline::filter<T, T> f()
  {
    return *this;
  }
};

//----------------------------------------------------------------------------
struct ohlc_close
{
  // returns the close price of OHLC candle
  double operator()(const QwtOHLCSample& ohlc) const
  {
    return ohlc.close;
  };

  pipeline::filter<double, const QwtOHLCSample&> f()
  {
    return *this;
  }
};

//----------------------------------------------------------------------------
struct cross
{
  cross()
    : last_(false)
    , first_(true)
  {
  }

  buy_sell_type operator()(double v1, double v2)
  {
    buy_sell_type e = buy_sell_type::no_event;
    if (first_)
    {
      first_ = false;
    }
    else if ((v1 > v2) != last_)
    {
      if (v1 > v2)
        e = buy_sell_type::buy_event;
      if (v1 < v2)
        e = buy_sell_type::sell_event;
    }
    last_ = (v1 > v2);
    return e;
  }

  pipeline::filter<buy_sell_type, double, double> f()
  {
    return *this;
  }

  private:
  //
  bool last_;
  bool first_;
};

//----------------------------------------------------------------------------
struct zero_cross
{
  zero_cross()
    : last_(false)
    , first_(true)
  {
  }

  buy_sell_type operator()(double v1)
  {
    buy_sell_type e = buy_sell_type::no_event;
    if (first_)
    {
      first_ = false;
    }
    else if ((0 > v1) != last_)
    {
      if (0 > v1)
        e = buy_sell_type::buy_event;
      if (0 < v1)
        e = buy_sell_type::sell_event;
    }
    last_ = (0 > v1);
    return e;
  }

  pipeline::filter<buy_sell_type, double> f()
  {
    return *this;
  }

  private:
  //
  bool last_;
  bool first_;
};

//----------------------------------------------------------------------------
struct difference
{
  difference() {}

  double operator()(double v1, double v2)
  {
    return (v1 - v2);
  }

  pipeline::filter<double, double, double> f()
  {
    return *this;
  }
};

//----------------------------------------------------------------------------
// Remove consecutive repetitions from a stream.
// used by cross detector to filter out change from true->false etc
template <typename T>
struct unique
{
  explicit unique()
    : first_(false)
    , prev_()
  {
  }

  std::optional<T> operator()(T value)
  {
    if (first_ || (value != prev_))
    {
      first_ = false;
      return std::optional<T>(prev_ = value);
    }
    return std::optional<T>{};
  }

  pipeline::filter<std::optional<T>, T> f()
  {
    return *this;
  }

  private:
  bool first_;
  T prev_;
};

//----------------------------------------------------------------------------
//
// class to hold our stream based data
//
//class trade_filter {
//public:
//    trade_filter();
//    virtual ~trade_filter() {}
//    //
//    void process(const QwtOHLCSample &ohlc);
//    //
//    volume_weighted_moving_average rolling_average_;
//    //
//    ohlc_input ohlc_in_;
//    pipeline::input<const QwtOHLCSample&> sample_input_;
//    //
//    exponential_moving_average ema_1;
//    exponential_moving_average ema_10;
//    //
//    pipeline::input<void> cross_detector_;
//};

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
using price_type = pipeline::pfunc<double>;
using event_type = pipeline::pfunc<trade_event>;
using ohlc_input_type = pipeline::input<const QwtOHLCSample&>;
using time_input_type = pipeline::input<double>;

//----------------------------------------------------------------------------
void make_heikin_ashi_pipeline(ohlc_input_type& ohlc_input, time_input_type& time_input,
  const candle_res& res, std::vector<event_type>& event_pipelines,
  std::vector<price_type>& price_pipelines);
//----------------------------------------------------------------------------
void make_moving_average_gradient(ohlc_input_type& ohlc_input, time_input_type& time_input, int N,
  std::vector<event_type>& event_pipelines, std::vector<price_type>& price_pipelines);
//----------------------------------------------------------------------------
void make_moving_average_cross(ohlc_input_type& ohlc_input, time_input_type& time_input, int N,
  int M, std::vector<event_type>& event_pipelines, std::vector<price_type>& price_pipelines);
//----------------------------------------------------------------------------
void make_MACD(ohlc_input_type& ohlc_input, time_input_type& time_input, int N, int M, int O,
  std::vector<event_type>& event_pipelines, std::vector<price_type>& price_pipelines,
  std::vector<price_type>& filter_pipelines);
