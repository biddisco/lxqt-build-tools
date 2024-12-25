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
class QwtPlotCurve;

namespace indicators {

  struct indicator_ptr
  {
    // // ----------------------------------------------------------------------------
    // ~indicator_API_binding()
    // {
    //   using namespace grox::debug;
    //   for (auto d : alg_.get_inputs())
    //   {
    //     std::string id = subscription_name();
    //     indicator_dbg<0>.debug(str<>("UnSubscribing"), id, d.dataset_->get_resolution());
    //     d.dataset_->new_data_subscribers_.unsubscribe(id);
    //   }
    // }

    // // ----------------------------------------------------------------------------
    // std::string subscription_name()
    // {
    //   return alg_.get_name() + "-" + std::to_string((uintptr_t) (&alg_));
    // }

    // ----------------------------------------------------------------------------
    indicators::indicator_base* indicator() const
    {
      return dynamic_cast<indicator_base*>(algorithm_.get());
    }

    // ----------------------------------------------------------------------------
    indicators::algorithm_base* ptr() const { return algorithm_.get(); }

    // // ----------------------------------------------------------------------------
    // void register_callbacks() override
    // {
    //   // register a handler to make sure we pickup updates to datasets
    //   using namespace grox::debug;
    //   for (auto d : alg_.get_inputs())
    //   {
    //     std::string id = subscription_name();
    //     indicator_dbg<0>.debug(str<>("Subscribing"), id, d.dataset_->get_resolution());
    //     d.dataset_->new_data_subscribers_.subscribe(id, [this](std::uint64_t N) {
    //       indicator_dbg<0>.debug(str<>(alg_.get_name().c_str()), "new samples", ffmt<dec4>(N));
    //       // todo - only call if all inputs are updated
    //       call_operator(N);
    //     });
    //   }
    // }

    // ----------------------------------------------------------------------------
    indicator_ptr(algorithm_ptr ap, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_)
    {
      // if this pointer is an indicator_base type
      indicator_base* ip = dynamic_cast<indicator_base*>(ap.get());
      if (ip)
      {
        algorithm_ = ip->create(ip, hdf5_ohlc_);
        std::uint64_t N = indicator()->get_inputs()[0].samples_;
        indicator()->execute(N);
        // hook updates
        //      binding->register_callbacks();
      }
      else { algorithm_ = ap->create(ap.get()); }
    }

    // // ----------------------------------------------------------------------------
    // /// constructor - creates the internal vtable enabled object
    // template <typename Algorithm>
    // indicator_ptr(Algorithm const& alg)
    // {
    //   // this is backwards - the shared pointer holds the API binding
    //   // instead of the algorithm - the create function returns a shared_pointer
    //   // to an algorithm, so we copy it out and throw away the shared wrapper
    //   binding = std::make_shared<indicator_API_binding<Algorithm>>(alg);
    //   //  @todo : redo the create function to fix this
    //   std::shared_ptr<Algorithm> temp = alg.create(alg);
    //   // copy through and then let the other go
    //   std::dynamic_pointer_cast<indicator_API_binding<Algorithm>>(binding)->alg_ = *temp;

    //   // // iterate over the input dataset(S), executing the algorithm for each point
    //   // std::uint64_t N = temp->get_inputs()[0].samples_;
    //   // if ((N == std::numeric_limits<std::uint64_t>::max()) ||
    //   //     (N > temp->get_inputs()[0].dataset_->size()))
    //   //   N = 0;
    //   // call_operator(N);
    //   // // hook updates
    //   // binding->register_callbacks();
    // }

    // ----------------------------------------------------------------------------
    ~indicator_ptr() {}

    // ----------------------------------------------------------------------------
    // void call_operator(std::uint64_t N) { binding->call_operator(N); }

    // ----------------------------------------------------------------------------
    // indicators::indicator_base* ptr() const { return binding->ptr(); }

    // ----------------------------------------------------------------------------
    algorithm_ptr algorithm_;
    indicator_plot* plot{nullptr};
    std::vector<QwtPlotCurve*> curves;
    bool visibility_{true};
  };
}    // namespace indicators
