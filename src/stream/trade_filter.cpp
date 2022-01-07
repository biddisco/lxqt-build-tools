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

