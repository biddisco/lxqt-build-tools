#include <functional>
#include <random>
//
#include "trade_filter.hpp"
#include "src/demangle_helper.hpp"
#include "src/print.hpp"
#include "src/json_types.hpp"
//
namespace ba = boost::accumulators;
namespace bt = ba::tag;
typedef ba::accumulator_set < double, ba::stats <bt::rolling_mean > > MeanAccumulator;

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Functor that prints whatever it gets.
// Streamify<print> is a stream function that prints every
// element of a stream.
struct print
{
    template<class Sig> struct result;

    template<class This,typename T>
    struct result<This(T)>
    {
        typedef T type;
    };

    template<typename T>
    typename result<print(T)>::type
    operator()(const T& value) const
    {
        std::cout << value << std::endl;
        return value;
    }
};

//----------------------------------------------------------------------------
// Return whatever you got as a string. This is useful
// for printing sub-expression results within a string
// (converting to string allows + with other strings).
struct as_string
{
    template<class Sig> struct result;

    template<class This,typename T>
    struct result<This(T)>
    {
        typedef std::string type;
    };

    template<typename T>
    typename result<print(T)>::type
    operator()(const T& value) const
    {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
};

//----------------------------------------------------------------------------
// Print an alert when a cross comes. Value indicates
// the type of the cross.
struct cross_alert
{
    bool operator()(const bool is_golden_cross)
    {
        if (is_golden_cross)
            std::cout << "Golden cross detected!" << std::endl;
        else
            std::cout << "Death cross detected!" << std::endl;

        return is_golden_cross;
    }

    pipeline::filter<bool,bool> f() { return *this; }
};

//----------------------------------------------------------------------------
//std::random_device rd;
//std::default_random_engine eng(rd());
//std::uniform_real_distribution<double> distr(0.01, 0.01);


//----------------------------------------------------------------------------
void make_heikin_ashi_pipeline(ohlc_input_type &ohlc_input,
                                     time_input_type &time_input,
                                     const candle_res &res,
                                     std::vector<event_type> &event_pipelines,
                                     std::vector<price_type> &price_pipelines)
{
    auto pipeline = ((ohlc_input | ohlc_candlemaker(res, ohlc_chart_data::minute).f() | ohlc_heikin_ashi().f() | heikin_ashi_transition().f())
                     +
                    (time_input | dummy<double>().f()))
                    | add_time_filter().f();
    event_pipelines.push_back(pipeline);
    //
    int N = res.res_ / ohlc_chart_data::minute;
    auto pipeline2 = ((ohlc_input | volume_weighted_moving_average(N).f()));
    price_pipelines.push_back(pipeline2);
}

//----------------------------------------------------------------------------
void make_moving_average_gradient(ohlc_input_type &ohlc_input,
                                        time_input_type &time_input,
                                        int N,
                                        std::vector<event_type> &event_pipelines,
                                        std::vector<price_type> &price_pipelines)
{
    auto pipeline = ((ohlc_input | volume_weighted_moving_average(N).f() | gradient_change().f())
                    +
                     (time_input | dummy<double>().f()))
                     | add_time_filter().f();
    event_pipelines.push_back(pipeline);
    //
    auto pipeline2 = ((ohlc_input | volume_weighted_moving_average(N).f()));
    price_pipelines.push_back(pipeline2);
}

//----------------------------------------------------------------------------
void make_moving_average_cross(ohlc_input_type &ohlc_input,
                                     time_input_type &time_input,
                                     int N,
                                     int M,
                                     std::vector<event_type> &event_pipelines,
                                     std::vector<price_type> &price_pipelines)
{
#define VWMA volume_weighted_moving_average
    auto pipeline = ((((ohlc_input | VWMA(N).f())
                        +
                       (ohlc_input | VWMA(M).f()))
                      | cross().f())
                     + (time_input | dummy<double>().f()))
                        | add_time_filter().f();
    event_pipelines.push_back(pipeline);
    //
    auto pipeline2 = ((ohlc_input | VWMA(N).f()));
    price_pipelines.push_back(pipeline2);
    auto pipeline3 = ((ohlc_input | VWMA(M).f()));
    price_pipelines.push_back(pipeline3);
}

//----------------------------------------------------------------------------
struct macd_internal {
    ohlc_input_type &ohlc_input;
    time_input_type &time_input;
    volume_weighted_moving_average ma_fast;
    volume_weighted_moving_average ma_slow;
    moving_average av3;
    pipeline::pfunc<double> p_fast;
    pipeline::pfunc<double> p_slow;
    zero_cross cross;
    add_time_filter tfilter;
    //
    macd_internal(ohlc_input_type &input1, time_input_type &input2, int M, int N, int O)
        : ohlc_input(input1)
        , time_input(input2)
        , ma_fast(M)
        , ma_slow(N)
        , av3(0)
        , tfilter()
    {
        p_fast = ohlc_input | ma_fast.f();
        p_slow = ohlc_input | ma_slow.f();
    }

    trade_event operator()() {
        double v1 = p_fast();
        double v2 = p_slow();
        double macd = difference()(v1,v2);
        double signal = av3(macd);
        double trigger = difference()(signal, macd);
        buy_sell_type bs = cross(trigger);
        trade_event e = tfilter(bs, time_input());
        return e;
    }

    pipeline::filter<trade_event> f() { return *this; }
};

//----------------------------------------------------------------------------
struct macd_signal {
    ohlc_input_type &ohlc_input;
    time_input_type &time_input;
    volume_weighted_moving_average ma_fast;
    volume_weighted_moving_average ma_slow;
    moving_average av3;
    pipeline::pfunc<double> p_fast;
    pipeline::pfunc<double> p_slow;
    zero_cross cross;
    add_time_filter tfilter;
    //
    macd_signal(ohlc_input_type &input1, time_input_type &input2, int fast, int slow, int O)
        : ohlc_input(input1)
        , time_input(input2)
        , ma_fast(fast)
        , ma_slow(slow)
        , av3(0)
        , tfilter()
    {
        p_fast = ohlc_input | ma_fast.f();
        p_slow = ohlc_input | ma_slow.f();
    }

    trade_event operator()() {
        double f = p_fast();
        double s = p_slow();
        double macd = difference()(s,f);
        double signal = av3(macd);
        double trigger = difference()(signal, macd);
        buy_sell_type bs = cross(trigger);
        trade_event e = tfilter(bs, time_input());
        return e;
    }

    pipeline::filter<trade_event> f() { return *this; }
};

void make_MACD(ohlc_input_type &ohlc_input,
               time_input_type &time_input,
               int fast, int slow, int O,
               std::vector<event_type> &event_pipelines,
               std::vector<price_type> &price_pipelines,
               std::vector<price_type> &filter_pipelines)
{
    auto f = macd_internal(ohlc_input, time_input, fast, slow, O);
    event_pipelines.push_back(f);
    //
    auto pipeline2 = ((ohlc_input | VWMA(fast).f()));
    price_pipelines.push_back(pipeline2);
    auto pipeline3 = ((ohlc_input | VWMA(slow).f()));
    price_pipelines.push_back(pipeline3);
    auto pipeline4 = ((ohlc_input | VWMA(slow).f()) + (ohlc_input | VWMA(fast).f())) | difference().f() | moving_average(O).f();
    filter_pipelines.push_back(pipeline4);

//    price_pipelines.push_back(pipeline2);
//    auto pipeline3 = ((ohlc_input | volume_weighted_moving_average(M).f()));
//    price_pipelines.push_back(pipeline3);
}
