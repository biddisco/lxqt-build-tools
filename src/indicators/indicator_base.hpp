#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/timebased_chart_data.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_partitioner.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
// Macro to implement factory methods for indicator creation and execution
// Note: Registration is handled by plugins, not via static initializers
#define FACTORY_INDICATOR_CREATE(type, operator_type)                                              \
  public:                                                                                          \
  shared_indicator create(algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc)       \
      const override                                                                               \
  {                                                                                                \
    auto result = std::make_shared<type>();                                                        \
    *result = *dynamic_cast<type*>(alg);                                                           \
    result->hdf5_ohlc_ = hdf5_ohlc;                                                                \
    result->initialize();                                                                          \
    result->create_outputs(hdf5_ohlc);                                                             \
    result->register_callbacks();                                                                  \
    return result;                                                                                 \
  }                                                                                                \
  void execute_from(std::uint64_t N) override                                                      \
  {                                                                                                \
    if ((N == std::numeric_limits<std::uint64_t>::max()) || (N > get_input(0).dataset_->size()))   \
    {                                                                                              \
      N = 0;                                                                                       \
    }                                                                                              \
    call_helper<operator_type> helper;                                                             \
    helper.execute(N, this, [this](ohlctv_sample const& sample) { return (*this)(sample); });      \
  }                                                                                                \
  void execute_continue() override                                                                 \
  {                                                                                                \
    std::uint64_t N = 1;                                                                           \
    if ((N == std::numeric_limits<std::uint64_t>::max()) || (N > get_input(0).dataset_->size()))   \
      N = 0;                                                                                       \
    call_helper<operator_type> helper;                                                             \
    helper.execute(N, this, [this](ohlctv_sample const& sample) { return (*this)(sample); });      \
  }

// ----------------------------------------------------------------------------
namespace indicators {
  class indicator_base;
  using shared_indicator = std::shared_ptr<indicator_base>;

  struct candle_input_data
  {
    std::shared_ptr<ohlc_dataset_view> view_;
    ohlc_dataset* dataset_;
    std::uint64_t samples_;
  };

  struct orderbook_input_data
  {
    ohlc_dataset* exchange_;
    std::uint64_t samples_;
  };

  // ----------------------------------------------------------------------------
  //
  // ----------------------------------------------------------------------------
  class indicator_base : public algorithm_base
  {
public:
    /// by default indicators produce double precision output
    using input_type = candle_input_data;
    using output_type = point_chart_data;
    using operator_type = double;

protected:
    overlay_vector overlay_;

    /// list of input datasets
    std::vector<input_type> in_datasets_;
    std::vector<output_type*> out_datasets_;
    std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;
    // once the algorithm begins executing, the valid index stores the
    // next(input) index for which an output needs to be generated
    // (if 100 values are computed, {0..99} valid index will be 100, the next start point)
    std::uint64_t valid_index_;
    bool executing_;

public:
    using algorithm_base::initialize;

    // ----------------------------------------------------------------------------
    indicator_base(std::string const& name, std::string const& desc, overlay_vector const& overlay)
      : algorithm_base(name, desc)
      , overlay_(overlay)
      , valid_index_{std::numeric_limits<std::uint64_t>::max()}
      , executing_{false}
    {
    }

    // ----------------------------------------------------------------------------
    virtual ~indicator_base()
    {
      // NOTE: No logging in destructor - may be called during static destruction
      // when logging system is already destroyed
      for (auto d : get_inputs())
      {
        std::string id = subscription_name();
        d.dataset_->new_data_subscribers_.unsubscribe(id);
      }
    }

    // ----------------------------------------------------------------------------
    // factor create function
    virtual shared_indicator create(
        algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const = 0;

    // ----------------------------------------------------------------------------
    /// in principle an indicator can return multiple graph series, which might require
    /// different display types, currently they are all the same, so 'n' is ignored
    virtual overlay_type const get_overlay(int n) const { return overlay_[n]; }

    // ----------------------------------------------------------------------------
    virtual std::vector<candle_input_data> const& get_inputs() const { return in_datasets_; }

    virtual candle_input_data const& get_input(std::size_t i) const
    {
      if (i >= in_datasets_.size()) { throw std::runtime_error("Setup inputs/outputs"); }
      return in_datasets_[i];
    }

    virtual std::vector<output_type*>& get_outputs() { return out_datasets_; }

    virtual output_type* const get_output(std::size_t i) const
    {
      if (i >= out_datasets_.size()) { throw std::runtime_error("Setup inputs/outputs"); }
      return out_datasets_[i];
    }

    // ----------------------------------------------------------------------------
    // create a dataset for each indicator output
    // default implementation uses first input resolution and size
    virtual void create_outputs(std::shared_ptr<ohlc_dataset_view> view)
    {
      in_datasets_ = connect_candle_input_datasets(view);
      //
      candle_res const res = in_datasets_[0].dataset_->get_resolution();
      std::size_t const size = in_datasets_[0].dataset_->data().size();
      for (int i = 0; i < num_outputs(); ++i)
      {
        output_type* indicator_data = new output_type(res);
        indicator_data->data().reserve(size);
        out_datasets_.push_back(indicator_data);
      }
    }

    // ----------------------------------------------------------------------------
    // iterate over the parameters returned from an indicator selection dialog and
    // find the datasets of the right resolution in the datasets view
    std::vector<candle_input_data> connect_candle_input_datasets(
        std::shared_ptr<ohlc_dataset_view> view)
    {
      std::vector<candle_input_data> result;
      for (auto const& p : get_params())
      {
        if (param<candle_data> const* d = std::get_if<param<candle_data>>(&p))
        {
          auto dataset = view->get_dataset(d->get().res_);
          result.push_back({view, dataset, d->get().duration_});
        }
      }
      return result;
    }

    // ----------------------------------------------------------------------------
    void register_callbacks()
    {
      // register a handler to make sure we pickup updates to datasets
      for (auto d : get_inputs())
      {
        std::string id = subscription_name();
        GROX_LOG_DEBUG(
            indicator_log, "{:>20} {} {}", "Subscribing", id, d.dataset_->get_resolution().name_);
        // attach a callback that is triggered when new data arrives
        d.dataset_->new_data_subscribers_.subscribe(id, [this](std::uint64_t N) {
          GROX_LOG_DEBUG(indicator_log, "{:>20} new samples {:04d}", get_name().c_str(), N);
          // todo - only call if all inputs are updated
          execute_continue();
        });
      }
    }

    // ----------------------------------------------------------------------------
    // execute the algorithm from a start point N samples back from the end
    // this should only be used when starting an algorithm for the first time
    virtual void execute_from(std::uint64_t N) = 0;

    // execute the algorithm from wherever it last completed, until the end
    // (mmeaning if N new samples have been added to the input, execute them)
    virtual void execute_continue() = 0;

    // ----------------------------------------------------------------------------
    void call_operator_ohlc_1(std::uint64_t N, std::function<double(ohlctv_sample const&)> fn)
    {
      auto const input = get_input(0).dataset_;
      auto output = get_output(0);
      //
      auto i1 = (N == 0) ? input->data().begin() : std::prev(input->data().end(), N);
      for (auto it = i1; it != input->data().end(); ++it)
      {
        auto const& ohlc = *it;
        auto vals = fn(ohlc);
        QPointF xyval(ohlc.time, vals);
        output->data().push_back(xyval);
      }
    }

    // ----------------------------------------------------------------------------
    void call_operator_ohlc_v(
        std::uint64_t N, std::function<std::vector<float>(ohlctv_sample const&)> fn)
    {
      auto const input = get_input(0).dataset_;
      auto outputs = get_outputs();
      //
      auto i1 = (N == 0) ? input->data().begin() : std::prev(input->data().end(), N);
      for (auto it = i1; it != input->data().end(); ++it)
      {
        auto const& ohlc = *it;
        auto vals = fn(ohlc);
        for (int i = 0; i < num_outputs(); ++i)
        {
          QPointF xyval(ohlc.time, vals[i]);
          outputs[i]->data().push_back(xyval);
        }
      }
    }

    // ----------------------------------------------------------------------------
    // The N parameter requests the last N samples to be processed, so we compute
    // the indices from the end, not the start. We must be careful because
    // if new data is added to the input during processing, all iterators/indices
    // we be invalided and we must carefully track our position.
    // We iterate in chunks (locking and unlocking to allow other threads to make progress)
    // and if anything changes during processing, we recompute partitions/iterators
    // and resume.
    // Note we can accept new data appended to the input, but we cannot handle deletion
    // which will trigger an abort
    void call_operator_buy_sell(
        std::uint64_t N, std::function<buy_sell_point(ohlctv_sample const&)> fn)
    {
      std::uint64_t const chunksize = 10000;
      auto const input = get_input(0).dataset_;
      auto origin_size = input->size();
      auto outputs = get_outputs();
      //
      {
        // take the lock on the data before getting any pointers during initialization
        auto l = get_input(0).view_->take_readonly_lock(name_, "operator_buy_sell", "start");
        if (executing_) return;
        executing_ = true;
        std::uint64_t start_index;
        // Are we executing from some particular start point, or continuing a previous one
        if (valid_index_ == std::numeric_limits<std::uint64_t>::max())
        {
          // this is the first execution, compute the start point
          start_index = (input->size() - N);
          valid_index_ = start_index;
        }
        else
        {
          // resume from previous position
          start_index = valid_index_;
        }
        GROX_LOG_DEBUG(indicator_log, "{:>20} start_index {}", "operator_buy_sell", start_index);
        auto partitioner = block_partitioner(input->data(), start_index, chunksize);
        // we have extracted pointers, we can now unlock
        l.unlock();

        // iterate over the data in chunks, taking and releasing the lock on each chunk
        // and recomputing the partition  at the start of each chunk, in case the input data changed
        for (std::uint64_t p = 0; p < partitioner.num_partitions_; p++)
        {
          auto l = get_input(0).view_->take_readonly_lock(name_, "operator_buy_sell", p);
          if (input->size() < origin_size)
          {
            GROX_LOG_DEBUG(
                indicator_log, "{:>20} Data reduced Aborting {}", "operator_buy_sell", p);
            break;
          }
          // recompute partitions, just in case input data grew in size
          partitioner = block_partitioner(input->data(), start_index, chunksize);
          auto extent = partitioner.get_partition(p);
          for (auto it = extent.begin; it != extent.end; ++it)
          {
            auto const& ohlc = *it;
            auto vals = fn(ohlc);
            QPointF xyval(ohlc.time, vals.price_);
            if (vals.event_type_ == buy_sell_event_type::buy)
            {
              outputs[0]->data().push_back({ohlc.time, vals.event_price_});
              outputs[2]->data().push_back(xyval);
            }
            else if (vals.event_type_ == buy_sell_event_type::sell)
            {
              outputs[1]->data().push_back({ohlc.time, vals.event_price_});
              outputs[2]->data().push_back(xyval);
            }
            else if (vals.event_type_ == buy_sell_event_type::value)
            {
              outputs[2]->data().push_back(xyval);
            }
            else { outputs[2]->data().push_back(xyval); }
            {
              outputs[3]->data().push_back({ohlc.time, vals.value_});
              // outputs[4]->data().push_back({ohlc.time, vals.tokens_});
            }
            valid_index_++;
          }
          GROX_LOG_DEBUG(indicator_log, "{:>20} partition complete {}", "operator_buy_sell", p);
        }
      }
      executing_ = false;
    }
  };

  template <typename operator_result>
  struct call_helper
  {
    void execute(
        std::uint64_t N, indicator_base* a, std::function<operator_result(ohlctv_sample const&)> fn)
    {
      a->call_operator_ohlc_1(N, fn);
    }
  };

  template <>
  struct call_helper<std::vector<float>>
  {
    void execute(std::uint64_t N, indicator_base* a,
        std::function<std::vector<float>(ohlctv_sample const&)> fn)
    {
      a->call_operator_ohlc_v(N, fn);
    }
  };

  template <>
  struct call_helper<buy_sell_point>
  {
    void execute(
        std::uint64_t N, indicator_base* a, std::function<buy_sell_point(ohlctv_sample const&)> fn)
    {
      a->call_operator_buy_sell(N, fn);
    }
  };

}    // namespace indicators
