#pragma once

#include <memory>
#include <ostream>
#include <string>

// ----------------------------------------------------------------------------
/// An indicator_ref holds a reference to a sub-indicator prototype, allowing
/// indicators to compose other indicators as parameters. The sub-indicator's
/// own params are exposed in the GUI as a nested group.
///
/// Example usage in init_params():
///   param<indicator_ref>{"Fast EMA", {"Moving Average (Exponential)", fast_proto}}
///
/// The prototype_ is a shared_ptr<algorithm_base>, forward-declared here
/// to avoid circular includes (algorithm_base includes indicator_types which
/// uses the variant that contains indicator_ref).

namespace indicators {
  class algorithm_base;
}    // namespace indicators

struct indicator_ref
{
  /// The name of the sub-indicator type (e.g. "Moving Average (Exponential)")
  std::string indicator_name_;

  /// A configured prototype of the sub-indicator, holding its own param values.
  /// Each indicator_ref owns an independent copy so params can differ
  /// (e.g. fast EMA window=9 vs slow EMA window=21).
  std::shared_ptr<indicators::algorithm_base> prototype_;

  indicator_ref() = default;

  explicit indicator_ref(std::string name)
    : indicator_name_(std::move(name))
  {
  }

  indicator_ref(std::string name, std::shared_ptr<indicators::algorithm_base> proto)
    : indicator_name_(std::move(name))
    , prototype_(std::move(proto))
  {
  }

  friend std::ostream& operator<<(std::ostream& os, indicator_ref const& r)
  {
    return os << r.indicator_name_;
  }
};
