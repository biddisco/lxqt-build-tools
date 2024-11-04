#pragma once

#include <assert.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "debug/print.hpp"
#include "indicators/indicator_base.hpp"

class indicator_plot;
class timebased_data_curve;

namespace indicators {
  /// indicator_ptr - contains a shared_ptr to a vtable which invokes the indicator API
  struct indicator_ptr
  {
    // ----------------------------------------------------------------------------
    /// invokes indicator_API - vtable of functions
    struct indicator_API_vtable
    {
      virtual ~indicator_API_vtable() = default;
      // access the base pointer
      virtual indicators::indicator_base* ptr() = 0;
      // access the specialized operator overloads
      virtual void call_operator(std::uint64_t) = 0;
      //
      virtual void register_callbacks() = 0;
    };

    // ----------------------------------------------------------------------------
    /// indicator_API_binding - templated binding of type to parameter API
    template <typename Algorithm>
    struct indicator_API_binding : indicator_API_vtable
    {
      indicator_API_binding(Algorithm const& x)
        : alg_(x)
      {
      }

      // ----------------------------------------------------------------------------
      ~indicator_API_binding()
      {
        using namespace grox::debug;
        for (auto d : alg_.get_inputs())
        {
          indicator_dbg<0>.debug(str<>("UnSubscribing"), d.dataset_->get_resolution());
          d.dataset_->new_data_subscribers_.unsubscribe("indicator");
        }
      }

      // ----------------------------------------------------------------------------
      indicators::indicator_base* ptr() override { return &alg_; }

      // ----------------------------------------------------------------------------
      void register_callbacks() override
      {
        // register a handler to make sure we pickup updates to datasets
        using namespace grox::debug;
        for (auto d : alg_.get_inputs())
        {
          std::string id = alg_.get_name() + std::to_string((uintptr_t) (&alg_));
          indicator_dbg<0>.debug(str<>("Subscribing"), id, d.dataset_->get_resolution());
          d.dataset_->new_data_subscribers_.subscribe(id, [this](std::uint64_t N) {
            indicator_dbg<0>.debug(str<>(alg_.get_name().c_str()), "new samples", ffmt<dec4>(N));
            // todo - only call if all inputs are updated
            call_operator(N);
          });
        }
      }

      // ----------------------------------------------------------------------------
      void call_operator(std::uint64_t N) override { call_operator_impl(N); }

      // ----------------------------------------------------------------------------
      /// The algorithm might not return a single value, so we provide
      /// overloads that can handle vectors of values
      template <typename T = Algorithm,
          typename std::enable_if_t<std::is_same<typename T::result_type, double>::value, bool>
              Enable = false>
      void call_operator_impl(std::uint64_t N)
      {
        auto const input = alg_.get_inputs()[0].dataset_;
        auto output = alg_.get_outputs()[0];
        //
        auto i1 = (N == 0) ? input->data().begin() : std::prev(input->data().end(), N);
        for (auto it = i1; it != input->data().end(); ++it)
        {
          auto const& ohlc = *it;
          auto vals = alg_.operator()(ohlc);
          QPointF xyval(ohlc.time, vals);
          output->data().push_back(xyval);
        }
      }

      // ----------------------------------------------------------------------------
      template <typename T = Algorithm,
          typename std::enable_if_t<
              std::is_same<typename T::result_type, std::vector<float>>::value, bool>
              Enable = false>
      void call_operator_impl(std::uint64_t N)
      {
        auto const input = alg_.get_inputs()[0].dataset_;
        auto outputs = alg_.get_outputs();
        //
        auto i1 = (N == 0) ? input->data().begin() : std::prev(input->data().end(), N);
        for (auto it = i1; it != input->data().end(); ++it)
        {
          auto const& ohlc = *it;
          auto vals = alg_.operator()(ohlc);
          for (int i = 0; i < alg_.num_outputs(); ++i)
          {
            QPointF xyval(ohlc.time, vals[i]);
            outputs[i]->data().push_back(xyval);
          }
        }
      }

      Algorithm alg_;
    };

    // ----------------------------------------------------------------------------
    /// constructor - creates the internal vtable enabled object
    template <typename Algorithm>
    indicator_ptr(Algorithm const& alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_)
    {
      // this is backwards - the shared pointer holds the API binding
      // instead of the algorithm - the create function returns a shared_pointer
      // to an algorithm, so we copy it out and throw away the shared wrapper
      binding = std::make_shared<indicator_API_binding<Algorithm>>(alg);
      //  @todo : redo the create function to fix this
      std::shared_ptr<Algorithm> temp = alg.create(alg, hdf5_ohlc_);
      // copy through and then let the other go
      std::dynamic_pointer_cast<indicator_API_binding<Algorithm>>(binding)->alg_ = *temp;

      // iterate over the input dataset(S), executing the algorithm for each point
      std::uint64_t N = temp->get_inputs()[0].samples_;
      if ((N == std::numeric_limits<std::uint64_t>::max()) ||
          (N > temp->get_inputs()[0].dataset_->size()))
        N = 0;
      call_operator(N);
      // hook updates
      binding->register_callbacks();
    }

    // ----------------------------------------------------------------------------
    ~indicator_ptr() {}

    // ----------------------------------------------------------------------------
    void call_operator(std::uint64_t N) { binding->call_operator(N); }

    // ----------------------------------------------------------------------------
    indicators::indicator_base* ptr() const { return binding->ptr(); }

    // ----------------------------------------------------------------------------
    std::shared_ptr<indicator_API_vtable> binding;
    indicator_plot* plot{nullptr};
    std::vector<timebased_data_curve*> curves;
  };
}    // namespace indicators
