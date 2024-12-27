#pragma once

#include <memory>
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

  // ----------------------------------------------------------------------------
  class algorithm_base
  {
protected:
    /// generic vars that can be provided at construction time
    std::string name_;
    std::string description_;

    /// list of parameters/types that need to be supplied for GUI generation and execution
    param_list params_;

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
    std::string subscription_name()
    {
      return get_name() + "-" + std::to_string((uintptr_t) (this));
    }
  };

}    // namespace indicators
