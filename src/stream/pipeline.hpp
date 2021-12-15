#pragma once
//
#include <any>
#include <iostream>
#include <optional>
#include <tuple>
#include <vector>
//
#include "src/data/ohlc_datasets.hpp"
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
namespace pipeline {

    template <typename Out, typename ... Args>
//    using pfunc = std::function<Out(Args...)>;
    struct pfunc : std::function<Out(Args...)> {
        using std::function<Out(Args...)>::function;
    };


    // --------------------------------------------------------------
    template <typename Out>
    using input  = pfunc<Out>;

    // --------------------------------------------------------------
    template <typename... Args>
    using output = pfunc<void, Args...>;

    // --------------------------------------------------------------
    template <typename Out, typename... Args>
    using filter = pfunc<Out, Args...>;

/*
    template <typename Filter, typename Out, typename ... Args>
    struct shared_filter;

    // --------------------------------------------------------------
    template <typename Filter, typename Out>
    struct shared_filter<Filter, Out>
    {
        using shared_filter_type = shared_filter<Filter, Out>;

        // Makes a copy of the filter as a shared pointer
        shared_filter(Filter& f) {
            filter_ = std::make_shared<Filter>(f);
        };

        // provide default copy, move, assignment
        shared_filter(const shared_filter_type &) = default;
        shared_filter(shared_filter_type &&) = default;
        shared_filter_type & operator = (shared_filter_type &) = default;

        // call operator invokes our internal function object and returns its result
        Out operator () () const {
            return filter_->operator()();
        }

        Filter &target() {
            return filter_.get();
        }
    private:
        // the function object we are wrapping
        std::shared_ptr<Filter> filter_;
    };

    // --------------------------------------------------------------
    template <typename Filter,
              typename Out,
              typename ... Args>
    struct shared_filter
    {
        using shared_filter_type = shared_filter<Filter, Out, Args...>;

        // Makes a copy of the filter as a shared pointer
        shared_filter(Filter& f) {
            filter_ = std::make_shared<Filter>(f);
        };

        // provide default copy, move, assignment
        shared_filter(const shared_filter_type &) = default;
        shared_filter(shared_filter_type &&) = default;
        shared_filter_type & operator = (shared_filter_type &) = default;

        // call operator invokes our internal function object and returns its result
        Out operator () (Args... args) const {
            return filter_->operator()(args...);
        }

        Filter &target() {
            return filter_;
        }
    private:
        // the function object we are wrapping
        std::shared_ptr<Filter> filter_;
    };
*/

    // --------------------------------------------------------------
    // Make a shared filter from a filter
//    template <typename Filter>
//    [[nodiscard]] auto make(Filter&& f) {
//        using Out  = typename Filter::Out;
//        using Args = typename Filter::Args;
//        if constexpr (std::is_same<void, Args>::value) {
//            return input<Filter, Out>(f);
//        }
//        else {
//            return shared_filter<Filter, Out, Args>(f);
//        }
//    }


    // --------------------------------------------------------------
    // pipeline operation : connect an input to a filter
    // return a function of type function<Output(Input /*=void*/)>
    // which Args a filter<Out,In>, void input gives filter<Out>
    template <typename Out, typename In>
    decltype (auto) operator | (pipeline::input<In> ip, pipeline::filter<Out, In> f) {
        pipeline::input<Out> new_function = [ip, f]() {
            In temp = ip();
            return f(temp);
        };
        return new_function;
    }

    // --------------------------------------------------------------
    // pipeline operation : Connect two filters
    // return a function of type function<Output(inputs...)>
    template <typename... In, typename Mid, typename Out>
    decltype (auto) operator | (pipeline::filter<Mid, In...> ip, pipeline::filter<Out, Mid> op) {
        pipeline::filter<Out, In...> new_function = [ip, op](In... args) {
            return op( ip(std::forward<In>(args)...) );
        };
        return new_function;
    }

    // --------------------------------------------------------------
    // pipeline operation : connect a filter to an output
    // return a function of type pfunc<void(void)>
    template <typename Out>
    decltype (auto) operator | (pipeline::filter<Out> ip, pipeline::output<Out> op) {
        pipeline::output<> new_function = [ip, op]() {
            op(ip());
        };
        return new_function;
    }

    // --------------------------------------------------------------
    // take a Joiner and pass it into a filter
    template <typename Joiner,
              template <typename, typename...> typename Filter,
              typename Out,
              typename... Args>
    struct filter_from_joiner
    {
        using OutputType = typename Filter<Out, Args...>::OutputType;

        filter_from_joiner() = default;
        filter_from_joiner(const filter_from_joiner &) = default;

        filter_from_joiner(Joiner joiner, Out f)
            : joiner_(joiner)
            , filter_(f)
        {}

        OutputType operator()() {
            joiner_.execute();
            const auto outputs = joiner_.outputs();
            return (std::apply(filter_, outputs));
        }

        Joiner                         joiner_;
        pipeline::filter<Out, Args...> filter_;
    };

    // --------------------------------------------------------------
    // execute each function stored in a tuple of functions
    // and store the result in a new tuple of results
    template <class Tuple, class Functions, std::size_t... I>
    void for_each_impl(Tuple&& tuple, Functions&& func, std::index_sequence<I...>)
    {
        ((std::get<I>(tuple) = std::get<I>(func)()), ...);
    }

    template <class Tuple, class Functions>
    constexpr decltype(auto) for_each(Tuple&& tuple, Functions&& f)
    {
        return for_each_impl(std::forward<Tuple>(tuple),
                             std::forward<Functions>(f),
                             std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
    }

    // --------------------------------------------------------------
    template <typename In1, typename In2, typename... Filters>
    struct filter_joiner
    {
        filter_joiner(Filters &&...Fs)
            : filters_(std::forward<Filters>(Fs)...) {}

        // --------------------------------------------------------------
        // connect a filter Joiner to a multi input filter
        // we capture *this by copy and make the lambda mutable to
        // ensure that we copy all contents into the new function
        // otherwise the execution may use a stale reference to data
        // --------------------------------------------------------------
        template <typename Out>
        pipeline::filter<Out> operator | (pipeline::filter<Out, In1, In2> f) {
            pipeline::filter<Out> new_function = [*this, f]() mutable
            {
                for_each(results_, filters_);
                return (std::apply(f, results_));
            };
            return new_function;
        }
        //
        std::tuple<Filters ...> filters_;
        std::tuple<typename Filters::result_type ...> results_;
    };

    // --------------------------------------------------------------
    template <typename Out1, typename Out2>
    decltype(auto) operator + (pipeline::input<Out1> &&f1, pipeline::input<Out2> &&f2) {
        return filter_joiner<Out1, Out2, pipeline::input<Out1>, pipeline::input<Out2>>(std::move(f1), std::move(f2));
    }
}
