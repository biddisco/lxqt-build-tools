/**
 * Python Plugin Infrastructure
 *
 * Provides runtime loading and execution of Python-based indicators
 * that have been registered through the plugin system.
 *
 * Usage:
 *   1. Write indicator in Python (see python_indicator_example.py)
 *   2. Place in plugins/python directory
 *   3. Plugin loader auto-discovers and loads
 */

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "indicators/indicator_base.hpp"
#include "indicators/indicator_registry.hpp"

#if __has_include(<spdlog/spdlog.h>)
# include <spdlog/spdlog.h>
# define GROX_PYPLUGIN_HAS_SPDLOG 1
#else
# define GROX_PYPLUGIN_HAS_SPDLOG 0
#endif

#if GROX_PYPLUGIN_HAS_SPDLOG
# define GROX_PYPLUGIN_TRACE(...)                                                                  \
   do {                                                                                            \
     spdlog::set_level(spdlog::level::debug);                                                      \
     spdlog::debug(__VA_ARGS__);                                                                   \
   } while (false)
#else
# define GROX_PYPLUGIN_TRACE(...)
#endif

namespace indicators { namespace python {

  class py_indicator_instance;
  class python_indicator_registry;
  class python_indicator_wrapper;

  /**
   * Wrapper class that adapts a Python indicator to the C++ indicator_base interface
   * 
   * This allows Python indicators to be registered with the main indicator registry
   * and appear in the GUI alongside C++ indicators.
   */
  class python_indicator_wrapper : public indicator_base
  {
public:
    python_indicator_wrapper();
    python_indicator_wrapper(std::string const& name, std::string const& description,
        std::string const& class_name, python_indicator_registry* registry);

    // indicator_base interface implementation
    FACTORY_INDICATOR_CREATE(python_indicator_wrapper, double)

    void initialize() override;
    void init_params() override;

    int num_inputs() const override { return 1; }
    int num_outputs() const override { return 1; }

    // Execute indicator on OHLCV sample
    double operator()(ohlctv_sample const& sample);

    double getLastResult() { return last_result_; }

private:
    std::string class_name_;
    python_indicator_registry* registry_;
    std::shared_ptr<py_indicator_instance> instance_;
    double last_result_ = 0.0;
  };

  /**
   * Registry for Python-based indicators
   *
   * Manages the lifecycle of Python interpreter and loaded indicator modules.
   * Singleton pattern for global access.
   */
  class python_indicator_registry
  {
public:
    static python_indicator_registry& instance()
    {
      static python_indicator_registry reg;
      return reg;
    }

    /**
     * Initialize Python runtime
     * Must be called once before loading any Python indicators
     */
    bool initialize();

    /**
     * Shutdown Python runtime
     * Should be called at application exit
     */
    void shutdown();

    /**
     * Load a Python indicator module from file
     * @param module_path Path to .py file or directory containing __init__.py
     * @return true on success
     */
    bool load_module(std::filesystem::path const& module_path);

    /**
     * Get registered Python indicator class
     * @param class_name Name of Python class
     * @return Pointer to class descriptor (opaque handle)
     */
    void* get_indicator_class(std::string const& class_name);

    /**
     * Create instance of Python indicator
     * @param class_name Name of Python indicator class
     * @return Instance handle
     */
    std::shared_ptr<py_indicator_instance> create_instance(std::string const& class_name);

    /**
     * Discover and load all Python indicators from a directory
     * @param directory Path to search for .py files
     * @return Number of indicators loaded
     */
    std::size_t load_indicators_from_directory(std::filesystem::path const& directory);

    /**
     * Get list of available Python indicator classes
     */
    std::vector<std::string> get_available_indicators() const;

    /**
     * Check if Python is initialized
     */
    bool is_initialized() const { return initialized_; }

    /**
     * Register loaded Python indicators with the main indicator registry
     * Creates C++ wrappers for each Python indicator and registers them
     * so they appear in the GUI dropdown.
     * 
     * @param indicator_registry Main indicator registry to register with
     * @return Number of indicators registered
     */
    std::size_t register_with_main_registry(indicators::indicator_registry& registry);

private:
    python_indicator_registry() = default;
    ~python_indicator_registry();

    bool initialized_ = false;
    std::map<std::string, void*> registered_classes_;
    void* py_main_module_ = nullptr;
  };

  /**
   * Instance wrapper for a Python indicator
   *
   * Wraps a Python object and exposes the indicator interface to C++
   */
  class py_indicator_instance
  {
public:
    py_indicator_instance(void* py_object);
    ~py_indicator_instance();

    /**
     * Call Python's compute_sample method
     */
    double compute_sample(
        double open, double high, double low, double close, double volume, uint64_t time);

    /**
     * Set Python object parameter
     */
    bool set_parameter(std::string const& name, double value);
    bool set_parameter(std::string const& name, int value);
    bool set_parameter(std::string const& name, std::string const& value);

    /**
     * Get Python object parameter
     */
    double get_double_parameter(std::string const& name);
    int get_int_parameter(std::string const& name);
    std::string get_string_parameter(std::string const& name);

    /**
     * Get underlying Python object handle
     */
    void* get_py_object() const { return py_object_; }

private:
    void* py_object_;    // PyObject* - kept as void* to avoid Python.h here
  };

}}    // namespace indicators::python
