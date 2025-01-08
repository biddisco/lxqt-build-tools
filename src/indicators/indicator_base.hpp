#pragma once

#include <memory>
#include <vector>

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
  std::shared_ptr<indicator_base> create(                                                          \
      algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const override            \
  {                                                                                                \
    auto result = std::make_shared<type>();                                                        \
    *result = *dynamic_cast<type*>(alg);                                                           \
    result->hdf5_ohlc_ = hdf5_ohlc;                                                                \
    result->create_outputs(hdf5_ohlc);                                                             \
    result->initialize();                                                                          \
    result->register_callbacks();                                                                  \
    return result;                                                                                 \
  }                                                                                                \
  void execute(std::uint64_t N) override                                                           \
  {                                                                                                \
    if ((N == std::numeric_limits<std::uint64_t>::max()) ||                                        \
        (N > get_inputs()[0].dataset_->size()))                                                    \
      N = 0;                                                                                       \
    call_helper<operator_type> helper;                                                             \
    helper.execute(N, this, [this](ohlctv_sample const& sample) { return (*this)(sample); });      \
  }                                                                                                \
  static inline indicator_type_inserter<type> inserter{};

// ----------------------------------------------------------------------------
namespace indicators {

  struct candle_input_data
  {
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
    virtual std::shared_ptr<indicator_base> create(
        algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const = 0;

    // ----------------------------------------------------------------------------
    /// in principle an indicator can return multiple graph series, which might require
    /// different display types, currently they are all the same, so 'n' is ignored
    virtual overlay_type const get_overlay(int n) const { return overlay_[n]; }

    // ----------------------------------------------------------------------------
    virtual std::vector<candle_input_data> const& get_inputs() const { return in_datasets_; }
    virtual std::vector<output_type*>& get_outputs() { return out_datasets_; }

    // ----------------------------------------------------------------------------
    // create a dataset for each indicator output
    // default implementation uses first input resolution and size
    void create_outputs(std::shared_ptr<ohlc_dataset_view> view)
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
        if (candle_data const* d = std::get_if<candle_data>(&p.value))
        {
          auto dataset = view->get_dataset(d->res_);
          result.push_back({dataset, d->numSamples_});
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
      auto const input = get_inputs()[0].dataset_;
      auto output = get_outputs()[0];
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
      auto const input = get_inputs()[0].dataset_;
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
    void call_operator_buy_sell(
        std::uint64_t N, std::function<buy_sell_point(ohlctv_sample const&)> fn)
    {
      auto const input = get_inputs()[0].dataset_;
      auto outputs = get_outputs();
      //
      auto i1 = (N == 0) ? input->data().begin() : std::prev(input->data().end(), N);
      for (auto it = i1; it != input->data().end(); ++it)
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
          QPointF trade(ohlc.time, vals.value_);
          outputs[3]->data().push_back(trade);
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
