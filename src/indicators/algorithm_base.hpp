#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>
//
#include "debug/demangle_helper.hpp"
#include "debug/print.hpp"
#include "indicators/indicator_types.hpp"

// ----------------------------------------------------------------------------
#define FACTORY_ALGORITHM_CREATE(type)                                                             \
  std::shared_ptr<algorithm_base> create(algorithm_base* alg) const override                       \
  {                                                                                                \
    auto result = std::make_shared<type>();                                                        \
    *result = *dynamic_cast<type*>(alg);                                                           \
    result->initialize();                                                                          \
    return result;                                                                                 \
  }

// ----------------------------------------------------------------------------
namespace indicators {
  class algorithm_base;
  using shared_algorithm = std::shared_ptr<algorithm_base>;

  // ----------------------------------------------------------------------------
  class algorithm_base
  {
protected:
    /// generic vars that can be provided at construction time
    std::string name_;
    std::string description_;

    /// list of parameters/types that need to be supplied for GUI generation and execution
    param_list params_;

    /// Duration (number of samples to process) — an execution concern, not an indicator parameter.
    /// Default is 5000; the GUI or backtester sets this before execution.
    std::uint64_t duration_{5000};

public:
    // ----------------------------------------------------------------------------
    algorithm_base(std::string const& name, std::string const& desc)
      : name_(name)
      , description_(desc)
    {
    }

    // ----------------------------------------------------------------------------
    virtual ~algorithm_base() {}

    // ----------------------------------------------------------------------------
    virtual std::shared_ptr<algorithm_base> create(algorithm_base* alg) const { return nullptr; }
    virtual void initialize() = 0;
    inline void initialize(indicators::param_list& p)
    {
      set_params(p);
      this->initialize();
    }
    virtual void init_params() = 0;

    // ----------------------------------------------------------------------------
    virtual std::string const get_name() const { return name_; }
    virtual std::string const get_description() const { return description_; }

    // ----------------------------------------------------------------------------
    virtual param_list const& get_params() const { return params_; }
    virtual void set_params(param_list const& p) { params_ = p; }

    // ----------------------------------------------------------------------------
    virtual int num_inputs() const { return 1; }
    virtual int num_outputs() const { return 1; }

    // ----------------------------------------------------------------------------
    /// The kind of this algorithm. Drives registry partitioning and GUI
    /// grouping. Defaults to graph; strategy and orderbook indicators override.
    /// Independent from overlay_type (a per-output display hint).
    virtual indicator_kind kind() const { return indicator_kind::graph; }

    // ----------------------------------------------------------------------------
    /// The trade action this algorithm performs, or std::nullopt if it is not
    /// a trading algorithm (graph/strategy indicators return nullopt). The
    /// trading launcher uses this to filter exchanges by capability via
    /// exchange->supported_trade_actions(). Only meaningful when kind() ==
    /// indicator_kind::orderbook.
    virtual std::optional<supported_trade_actions> trade_action() const { return std::nullopt; }

    // ----------------------------------------------------------------------------
    /// The implementation source of this algorithm. Orthogonal to kind() —
    /// kind is about behavior, source is about implementation. Defaults to
    /// cpp; python_indicator_wrapper overrides to python.
    virtual indicator_source source() const { return indicator_source::cpp; }

    // ----------------------------------------------------------------------------
    /// Describes each named output. Replaces the positional outputs[N]
    /// convention. The default returns an empty vector; indicator_base
    /// overrides to synthesize from num_outputs() + get_overlay(n) so existing
    /// indicators work without changes. Converted indicators override with
    /// explicit names.
    virtual output_descriptors get_output_descriptors() const { return {}; }

    // ----------------------------------------------------------------------------
    /// Clones this algorithm from its prototype form into a ready-to-execute
    /// instance. Replaces the create(alg*) + FACTORY_INDICATOR_CREATE macro
    /// pair. Default returns nullptr; converted indicators override. Existing
    /// indicators continue to use create(alg*) until converted in Phase 1.
    virtual shared_algorithm clone() const { return nullptr; }

    // ----------------------------------------------------------------------------
    /// Duration (sample count) is an execution property, not an indicator parameter
    std::uint64_t get_duration() const { return duration_; }
    void set_duration(std::uint64_t d) { duration_ = d; }

    // ----------------------------------------------------------------------------
    std::string subscription_name()
    {
      return get_name() + "-" + std::to_string((uintptr_t) (this));
    }
  };

}    // namespace indicators
