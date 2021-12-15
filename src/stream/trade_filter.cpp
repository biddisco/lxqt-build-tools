#include <functional>
#include <random>
//
#include "trade_filter.hpp"
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

//----------------------------------------------------------------------------
trade_filter::trade_filter()
    : rolling_average_(10)
    , ohlc_in_(QwtOHLCSample())
    , ema_1(1.0)
    , ema_10(1.0)
{
    sample_input_ = std::ref(ohlc_in_);
//    auto p1 = sample_input_ | ema_1.f();
//    auto p2 = sample_input_ | ema_10.f();

    pipeline::output<std::optional<bool>> alert_ = pipeline::output<std::optional<bool>>(
    [](std::optional<bool> b) {
        if (b.has_value()) {
            std::cout << "Cross detected" << std::endl;
        }
    });

    cross_detector_ = ((sample_input_ | ema_1.f()) + (sample_input_ | ema_10.f())) | cross().f() | unique<bool>().f() | alert_;

//    auto pp1 = (std::move(p1) + std::move(p2)) | cross().f();
//    pp1();

//    cross_detector_ = (std::move(p1) + std::move(p2)) | cross().f() | unique<bool>().f() | alert_;


    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_real_distribution<double> distr(0.01, 0.01);

//    ohlc_in_.set(QwtOHLCSample(distr(eng), distr(eng), distr(eng), distr(eng), distr(eng), distr(eng)));
//    cross_detector_();

//    ohlc.close = 0.50;
//    pipeline::input<QwtOHLCSample> test1 = std::ref(ohlc_in_);
//    pipeline::filter<double,QwtOHLCSample&> e1 = exponential_moving_average(1.0);
//    pipeline::filter<double,QwtOHLCSample&> e2 = exponential_moving_average(10.0);
//    pipeline::input<bool> test3 = [test1, e1, e2]() {
//        auto t1 = test1();
//        auto a1 = e1(t1);
//        auto a2 = e2(t1);
//        std::cout << a1 << " " << a2 << " " << std::endl;
//        return a1>=a2;
//    };
//    pipeline::filter<std::optional<bool>,bool> u1 = unique<bool>();

}

//----------------------------------------------------------------------------
void trade_filter::process(const QwtOHLCSample &ohlc)
{
    // update the rolling mean filter
    std::cout << "Volume MAV = " << hpx::debug::fp<5,7>(rolling_average_(ohlc.volume)) << std::endl << std::endl;

    ohlc_in_.set(ohlc);
    cross_detector_();

    // update the latest ohlc value input and execute connected pipelines
//    ohlcinput_.target().set(ohlc);
//    std::cout << "ema 1  " << pipeline_1_() << std::endl;
//    std::cout << "ema 10 " << pipeline_2_() << std::endl;
//    std::optional<bool> alert = cross_alerter_();
//    if (alert.has_value()) {
//        std::cout << "Alert" << std::endl;
//    }
}

