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
void make_MACD(ohlc_input_type &ohlc_input,
               time_input_type &time_input,
               const candle_res &res,
               int N, int M, int O,
               std::vector<event_type> &event_pipelines,
               std::vector<price_type> &price_pipelines)
{
//    auto p1 = ohlc_input | ohlc_candlemaker(res, ohlc_chart_data::minute).f();
//    auto p2 = p1 | volume_weighted_moving_average(N).f();
//    auto p3 = p1 | volume_weighted_moving_average(M).f();
//    auto p4 = (p1 + p2) | difference().f() | volume_weighted_moving_average(O).f();
//    auto p5 =
//                     + (time_input | dummy<double>().f()))
//                        | add_time_filter().f();
//    event_pipelines.push_back(pipeline);
//    //
//    auto pipeline2 = ((ohlc_input | volume_weighted_moving_average(N).f()));
//    price_pipelines.push_back(pipeline2);
//    auto pipeline3 = ((ohlc_input | volume_weighted_moving_average(M).f()));
//    price_pipelines.push_back(pipeline3);
}
