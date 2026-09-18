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
#include "debug/logging.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_partitioner.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
// Macro for converted indicators: implements clone(), create(), execute_from,
// and execute_continue via the new process_sample virtual.
// process_sample must be implemented by the indicator.
// The indicator must also implement get_output_descriptors() with explicit names.
#define FACTORY_INDICATOR_V2(type)                                                                 \
  public:                                                                                          \
  shared_algorithm clone() const override                                                          \
  {                                                                                                \
    auto result = std::make_shared<type>(*this);                                                   \
    result->initialize();                                                                          \
    return result;                                                                                 \
  }                                                                                                \
  shared_indicator create(algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc)       \
      const override                                                                               \
  {                                                                                                \
    auto result = std::make_shared<type>(*dynamic_cast<type*>(alg));                               \
    result->hdf5_ohlc_ = hdf5_ohlc;                                                                \
    result->initialize();                                                                          \
    result->create_outputs(hdf5_ohlc);                                                             \
    result->register_callbacks();                                                                  \
    return result;                                                                                 \
  }                                                                                                \
  void execute_from(std::uint64_t N) override                                                      \
  {                                                                                                \
    if (N == std::numeric_limits<std::uint64_t>::max())                                            \
      N = get_input(0).dataset_->size();                                                           \
    else if (N > get_input(0).dataset_->size())                                                    \
      N = get_input(0).dataset_->size();                                                           \
    execute_streaming(N);                                                                          \
  }                                                                                                \
  void execute_continue() override { execute_streaming(1); }

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
    std::vector<std::shared_ptr<output_type>> out_datasets_;
    std::shared_ptr<ohlc_dataset_view> hdf5_ohlc_;
    // once the algorithm begins executing, the valid index stores the
    // next(input) index for which an output needs to be generated
    // (if 100 values are computed, {0..99} valid index will be 100, the next start point)
    std::uint64_t valid_index_;
    bool executing_;
    bool callbacks_registered_{false};
    /// Set to true once Qwt curves have taken ownership of the raw
    /// point_chart_data* via setData(). When true, ~indicator_base()
    /// skips deleting the output data to avoid a double-free.
    bool outputs_released_{false};

public:
    using algorithm_base::create;
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
      if (callbacks_registered_)
      {
        for (auto d : get_inputs())
        {
          std::string id = subscription_name();
          try
          {
            d.dataset_->new_data_subscribers_.unsubscribe(id);
          }
          catch (std::exception const& e)
          {
            // Dataset may already be destroyed (dangling raw pointer) - best effort cleanup
            std::cout << "ERROR: Exception during indicator_base destruction unsubscribe, id: "
                      << id << " what: " << e.what() << std::endl;
          }
        }
      }
      // Output shared_ptrs have no-op deleters (see create_outputs). We must
      // manually delete the raw pointers, but ONLY if this is the last
      // reference (unique) AND Qwt curves haven't taken ownership.
      if (!outputs_released_)
      {
        for (auto& d : out_datasets_)
        {
          if (d.unique()) { delete d.get(); }
        }
      }
      out_datasets_.clear();
    }

    // ----------------------------------------------------------------------------
    // factor create function
    virtual shared_indicator create(
        algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const = 0;

    // ----------------------------------------------------------------------------
    /// in principle an indicator can return multiple graph series, which might require
    /// different display types, currently they are all the same, so 'n' is ignored
    virtual overlay_type get_overlay(int n) const { return overlay_[n]; }

    // ----------------------------------------------------------------------------
    /// Synthesizes output descriptors from num_outputs() + get_overlay(n) so
    /// existing indicators get named outputs without changes. Converted
    /// indicators override with explicit names.
    output_descriptors get_output_descriptors() const override
    {
      output_descriptors result;
      for (int i = 0; i < num_outputs(); ++i)
      {
        result.push_back({"output_" + std::to_string(i), get_overlay(i)});
      }
      return result;
    }

    // ----------------------------------------------------------------------------
    virtual std::vector<candle_input_data> const& get_inputs() const { return in_datasets_; }

    virtual candle_input_data const& get_input(std::size_t i) const
    {
      if (i >= in_datasets_.size()) { throw std::runtime_error("Setup inputs/outputs"); }
      return in_datasets_[i];
    }

    virtual std::vector<std::shared_ptr<output_type>> const& get_outputs() const
    {
      return out_datasets_;
    }

    virtual std::shared_ptr<output_type> get_output(std::size_t i) const
    {
      if (i >= out_datasets_.size()) { throw std::runtime_error("Setup inputs/outputs"); }
      return out_datasets_[i];
    }

    /// Mark outputs as released to Qwt. After this call, ~indicator_base()
    /// will not delete the output data, preventing a double-free: Qwt curves
    /// own the raw point_chart_data* via setData().
    void release_outputs() { outputs_released_ = true; }

    // ----------------------------------------------------------------------------
    bool callbacks_registered() const { return callbacks_registered_; }
    void set_callbacks_registered(bool registered) { callbacks_registered_ = registered; }

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
        // Use a no-op custom deleter so that ~indicator_base() can
        // selectively free the data (only when Qwt curves haven't
        // taken ownership via setData()).
        auto raw = new output_type(res);
        raw->data().reserve(size);
        out_datasets_.push_back(std::shared_ptr<output_type>(raw, [](output_type*) {}));
      }
    }

    // ----------------------------------------------------------------------------
    // iterate over the parameters returned from an indicator selection dialog and
    // find the datasets of the right resolution in the datasets view.
    // Duration comes from algorithm_base::duration_, not from the candle_data param.
    std::vector<candle_input_data> connect_candle_input_datasets(
        std::shared_ptr<ohlc_dataset_view> view)
    {
      std::vector<candle_input_data> result;
      for (auto const& p : get_params())
      {
        if (param<candle_data> const* d = std::get_if<param<candle_data>>(&p))
        {
          auto dataset = view->get_dataset(d->get().res_);
          result.push_back({view, dataset, get_duration()});
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
      callbacks_registered_ = true;
    }

    // ----------------------------------------------------------------------------
    // execute the algorithm from a start point N samples back from the end
    // this should only be used when starting an algorithm for the first time
    virtual void execute_from(std::uint64_t N) = 0;

    // execute the algorithm from wherever it last completed, until the end
    // (mmeaning if N new samples have been added to the input, execute them)
    virtual void execute_continue() = 0;

    // ----------------------------------------------------------------------------
    /// Per-sample computation. Converted indicators implement this; the base
    /// fans the result into named outputs via execute_streaming.
    ///   double                  -> single output (output 0)
    ///   std::span<float const>  -> multiple outputs (must match num_outputs())
    ///   buy_sell_point          -> strategy output (buy/sell/value/etc.)
    ///
    /// TODO(emit-to-sink): upgrade to pass an output_sink& so indicators push
    /// named outputs directly, enabling variable/sparse output counts.
    virtual sample_result process_sample(market_sample const& /*sample*/) { return 0.0; }

    // ----------------------------------------------------------------------------
    /// Pre-allocated buffer for multi-output indicators. Sized to
    /// num_outputs() at initialize() time. process_sample returns a span into
    /// this buffer; no per-sample allocation.
    std::vector<float> output_buffer_;

    // ----------------------------------------------------------------------------
    /// Streaming execution engine. Iterates the last N samples of the input,
    /// calling process_sample for each and fanning the result into named
    /// outputs. Handles chunked locking for thread-safety (supports input
    /// growth during iteration, aborts on shrink).
    ///
    /// This replaces call_operator_ohlc_1/v and call_operator_buy_sell.
    /// Dispatch on sample_result variant arm:
    ///   double           -> QPointF(time, val) into output 0
    ///   span<float const>-> QPointF(time, vals[i]) into each output i
    ///   buy_sell_point   -> buy into output 0, sell into output 1,
    ///                       av_price into 2, value into 3 (per get_output_descriptors)
    void execute_streaming(std::uint64_t N)
    {
      std::uint64_t const chunksize = 10000;
      auto const input = get_input(0).dataset_;
      auto origin_size = input->size();
      auto outputs = get_outputs();
      auto descriptors = get_output_descriptors();

      {
        auto l = get_input(0).view_->take_readonly_lock(name_, "execute_streaming", "start");
        if (executing_) return;
        executing_ = true;
        std::uint64_t start_index;
        if (valid_index_ == std::numeric_limits<std::uint64_t>::max())
        {
          start_index = (input->size() - N);
          valid_index_ = start_index;
        }
        else { start_index = valid_index_; }
        GROX_LOG_DEBUG(indicator_log, "{:>20} start_index {}", "execute_streaming", start_index);
        auto partitioner = block_partitioner(input->data(), start_index, chunksize);
        l.unlock();

        for (std::uint64_t p = 0; p < partitioner.num_partitions_; p++)
        {
          auto l = get_input(0).view_->take_readonly_lock(name_, "execute_streaming", p);
          if (input->size() < origin_size)
          {
            GROX_LOG_DEBUG(
                indicator_log, "{:>20} Data reduced Aborting {}", "execute_streaming", p);
            break;
          }
          partitioner = block_partitioner(input->data(), start_index, chunksize);
          auto extent = partitioner.get_partition(p);
          for (auto it = extent.begin; it != extent.end; ++it)
          {
            auto const& ohlc = *it;
            auto result = process_sample(market_sample{ohlc});
            //
            if (std::holds_alternative<double>(result))
            {
              double val = std::get<double>(result);
              outputs[0]->data().push_back({ohlc.time, val});
            }
            else if (std::holds_alternative<std::span<float const>>(result))
            {
              auto vals = std::get<std::span<float const>>(result);
              for (int i = 0; i < num_outputs() && i < static_cast<int>(vals.size()); ++i)
              {
                outputs[i]->data().push_back({ohlc.time, vals[i]});
              }
            }
            else if (std::holds_alternative<buy_sell_point>(result))
            {
              auto const& vals = std::get<buy_sell_point>(result);
              // find outputs by name from descriptors
              for (std::size_t i = 0; i < descriptors.size() && i < outputs.size(); ++i)
              {
                auto const& desc = descriptors[i];
                if (desc.name_ == "buy" && vals.event_type_ == buy_sell_event_type::buy)
                  outputs[i]->data().push_back({vals.event_time_, vals.event_price_});
                else if (desc.name_ == "sell" && vals.event_type_ == buy_sell_event_type::sell)
                  outputs[i]->data().push_back({vals.event_time_, vals.event_price_});
                else if (desc.name_ == "price")
                  outputs[i]->data().push_back({ohlc.time, vals.price_});
                else if (desc.name_ == "value")
                  outputs[i]->data().push_back({ohlc.time, vals.value_});
              }
            }
            valid_index_++;
          }
          GROX_LOG_DEBUG(indicator_log, "{:>20} partition complete {}", "execute_streaming", p);
        }
      }
      executing_ = false;
    }
  };

}    // namespace indicators
