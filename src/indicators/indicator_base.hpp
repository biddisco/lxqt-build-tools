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
#include "data/ohlc_utils.hpp"
#include "data/timebased_chart_data.hpp"
#include "debug/print.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
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
  void execute(std::uint64_t N) override                                                           \
  {                                                                                                \
    if ((N == std::numeric_limits<std::uint64_t>::max()) || (N > get_input(0).dataset_->size()))   \
      N = 0;                                                                                       \
    call_helper<operator_type> helper;                                                             \
    helper.execute(N, this, [this](ohlctv_sample const& sample) { return (*this)(sample); });      \
  }                                                                                                \
  static inline indicator_type_inserter<type> inserter{};

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

public:
    using algorithm_base::initialize;

    // ----------------------------------------------------------------------------
    indicator_base(std::string const& name, std::string const& desc, overlay_vector const& overlay)
      : algorithm_base(name, desc)
      , overlay_(overlay)
    {
    }

    // ----------------------------------------------------------------------------
    virtual ~indicator_base()
    {
      using namespace grox::debug;
      for (auto d : get_inputs())
      {
        std::string id = subscription_name();
        indicator_dbg<0>.debug(ffmt<s20>("UnSubscribing"), id, d.dataset_->get_resolution());
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
      using namespace grox::debug;
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
      using namespace grox::debug;
      for (auto d : get_inputs())
      {
        std::string id = subscription_name();
        indicator_dbg<0>.debug(ffmt<s20>("Subscribing"), id, d.dataset_->get_resolution());
        // attach a callback that is triggered when new data arrives
        d.dataset_->new_data_subscribers_.subscribe(id, [this](std::uint64_t N) {
          indicator_dbg<0>.debug(ffmt<s20>(get_name().c_str()), "new samples", ffmt<dec4>(N));
          // todo - only call if all inputs are updated
          execute(N);
        });
      }
    }

    // ----------------------------------------------------------------------------
    virtual void execute(std::uint64_t N) = 0;

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

    template <typename Container>
    struct block_partitioner
    {
      using Iterator = Container::const_iterator;

      struct extent
      {
        Iterator begin;
        Iterator end;
      };

      block_partitioner(Container const& input, std::uint64_t start, std::uint64_t chunksize)
      {
        chunksize_ = chunksize;
        num_partitions_ = std::ceil(static_cast<double>(input.size() - start) / chunksize);
        // set extent from begin of partition 0, the end of data
        origin_.begin = std::next(input.begin(), start);
        origin_.end = input.end();
        indicator_dbg<5>.debug(ffmt<s20>("Partition create"), ffmt<dec3>(num_partitions_),
            static_cast<void const*>(&*input.begin()), static_cast<void const*>(&*input.end()),
            ffmt<dec8>(input.size()));
      }

      extent get_partition(int piece) const
      {
        Iterator begin = std::next(origin_.begin, piece * chunksize_);
        Iterator end = std::min(std::next(origin_.begin, (piece + 1) * chunksize_), origin_.end);
        indicator_dbg<5>.debug(ffmt<s20>("Partition get"), ffmt<dec3>(piece),
            fmt::format(
                "{},{}", static_cast<void const*>(&*begin), static_cast<void const*>(&*end)),
            begin - origin_.begin, end - origin_.begin);
        return {begin, end};
      }
      //
      std::uint64_t chunksize_;
      std::uint64_t num_partitions_;
      extent origin_;
    };

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
        indicator_dbg<5>.debug(ffmt<s20>("operator_buy_sell"), "iterations", N);
        std::uint64_t start_index = (input->size() - N);
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
            indicator_dbg<5>.debug(ffmt<s20>("operator_buy_sell"), "Data reduced", "Aborting", p);
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
              outputs[0]->data().push_back(xyval);
              outputs[2]->data().push_back(xyval);
            }
            else if (vals.event_type_ == buy_sell_event_type::sell)
            {
              outputs[1]->data().push_back(xyval);
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
          }
          indicator_dbg<2>.debug(ffmt<s20>("operator_buy_sell"), "partition complete", p);
        }
      }
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
