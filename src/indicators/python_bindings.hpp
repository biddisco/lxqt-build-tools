#pragma once

/**
 * Python bindings for indicator system
 *
 * Provides C++ wrapper classes and utilities for exposing indicators to Python
 * through the plugin architecture. This allows Python-based indicators to be
 * dynamically created and registered.
 */

#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
//
#include "data/ohlc_utils.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace indicators { namespace python {

  // Forward declarations
  class py_indicator_base;

  /**
   * @brief Python-wrappable algorithm base class
   *
   * Provides a simpler C++ interface that can be wrapped with pybind11
   * for Python subclassing. Derives from the real algorithm_base.
   */
  class py_algorithm_base : public algorithm_base
  {
public:
    py_algorithm_base(std::string const& name, std::string const& desc)
      : algorithm_base(name, desc)
    {
    }

    virtual ~py_algorithm_base() = default;

    // Allow Python to override these methods
    virtual void initialize() override = 0;
    virtual void init_params() override = 0;

    // Helper for Python to add parameters
    void add_double_param(std::string const& name, double default_value)
    {
      params_.push_back(param<double>{QString::fromStdString(name), default_value});
    }

    void add_int_param(std::string const& name, int default_value)
    {
      params_.push_back(param<int>{QString::fromStdString(name), default_value});
    }

    void add_string_param(std::string const& name, std::string const& default_value)
    {
      params_.push_back(param<std::string>{QString::fromStdString(name), default_value});
    }

    void add_bool_param(std::string const& name, bool default_value)
    {
      params_.push_back(param<bool>{QString::fromStdString(name), default_value});
    }

    // Helper for Python to get parameters
    std::optional<double> get_double_param(std::size_t index) const
    {
      if (index >= params_.size()) return std::nullopt;
      if (auto* p = std::get_if<param<double>>(&params_[index])) return p->get();
      return std::nullopt;
    }

    std::optional<int> get_int_param(std::size_t index) const
    {
      if (index >= params_.size()) return std::nullopt;
      if (auto* p = std::get_if<param<int>>(&params_[index])) return p->get();
      return std::nullopt;
    }

    std::optional<std::string> get_string_param(std::size_t index) const
    {
      if (index >= params_.size()) return std::nullopt;
      if (auto* p = std::get_if<param<std::string>>(&params_[index])) return p->get();
      return std::nullopt;
    }

    std::optional<bool> get_bool_param(std::size_t index) const
    {
      if (index >= params_.size()) return std::nullopt;
      if (auto* p = std::get_if<param<bool>>(&params_[index])) return p->get();
      return std::nullopt;
    }

    std::size_t get_num_params() const { return params_.size(); }
  };

  /**
   * @brief Python-wrappable indicator base class
   *
   * Provides simplified interface for Python-based indicators.
   * Python code derives from this class and implements the operator().
   */
  class py_indicator_base : public indicator_base
  {
public:
    py_indicator_base(std::string const& name, std::string const& desc,
        overlay_vector const& overlay = {overlay_type::price})
      : indicator_base(name, desc, overlay)
    {
    }

    virtual ~py_indicator_base() = default;

    // Python should implement this to return the computed result
    virtual double compute_sample(ohlctv_sample const& sample) = 0;

    // Wrapper implementation that calls Python's compute_sample
    inline double operator()(ohlctv_sample const& sample) { return compute_sample(sample); }

    // Expose parameter setters for Python
    void set_double_param(std::size_t index, double value)
    {
      if (index >= params_.size()) throw std::runtime_error("Invalid parameter index");
      if (auto* p = std::get_if<param<double>>(&params_[index]))
        p->put(value);
      else
        throw std::runtime_error("Parameter type mismatch");
    }

    void set_int_param(std::size_t index, int value)
    {
      if (index >= params_.size()) throw std::runtime_error("Invalid parameter index");
      if (auto* p = std::get_if<param<int>>(&params_[index]))
        p->put(value);
      else
        throw std::runtime_error("Parameter type mismatch");
    }

    // OHLCV data access helpers for Python
    struct sample_data
    {
      double open, high, low, close, volume;
      std::uint64_t time;

      static sample_data from_ohlctv(ohlctv_sample const& s)
      {
        return sample_data{s.open_, s.high_, s.low_, s.close_, s.volume_, s.time_};
      }
    };

    // These will be implemented via FACTORY_INDICATOR_CREATE macro
    virtual shared_indicator create(
        algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const = 0;
  };

  /**
   * @brief Simple data holder for indicator output
   *
   * Used by Python indicators to push computed values
   */
  class indicator_output_buffer
  {
private:
    std::vector<double> data_;

public:
    void push(double value) { data_.push_back(value); }

    double get_last() const
    {
      if (data_.empty()) return 0.0;
      return data_.back();
    }

    std::vector<double> const& get_data() const { return data_; }

    void clear() { data_.clear(); }

    std::size_t size() const { return data_.size(); }
  };

  /**
   * @brief Helper for parameter type conversion
   *
   * Converts between Python-friendly types and the variant-based param system
   */
  struct param_helper
  {
    static std::string to_json_string(param_list const& params)
    {
      std::ostringstream oss;
      oss << "[\n";
      for (std::size_t i = 0; i < params.size(); ++i)
      {
        oss << "  {\"index\": " << i << ", ";
        std::visit(
            [&oss](auto const& param) {
              using ParamType = std::decay_t<decltype(param)>;
              if constexpr (std::is_same_v<ParamType, param<double>>)
              {
                oss << "\"type\": \"double\", \"value\": " << std::fixed << std::setprecision(6)
                    << param.get() << "}";
              }
              else if constexpr (std::is_same_v<ParamType, param<int>>)
              {
                oss << "\"type\": \"int\", \"value\": " << param.get() << "}";
              }
              else if constexpr (std::is_same_v<ParamType, param<std::string>>)
              {
                oss << "\"type\": \"string\", \"value\": \"" << param.get() << "\"}";
              }
              else if constexpr (std::is_same_v<ParamType, param<bool>>)
              {
                oss << "\"type\": \"bool\", \"value\": " << (param.get() ? "true" : "false") << "}";
              }
              else { oss << "\"type\": \"unknown\"}"; }
            },
            params[i]);
        if (i < params.size() - 1) oss << ",";
        oss << "\n";
      }
      oss << "]";
      return oss.str();
    }
  };

}}    // namespace indicators::python
