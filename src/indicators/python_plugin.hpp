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

#include "debug/logging.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_registry.hpp"

// ----------------------------------------------------------------------------
static auto py_plug_log = grox::log::create("pyplugin");

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

    // Custom factory create that calls init_params() before initialize()
    shared_indicator create(
        algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const override;
    void execute_from(std::uint64_t N) override;
    void execute_continue() override;

    /// Per-sample computation: delegates to the Python compute_sample method
    sample_result process_sample(market_sample const& sample) override;

    void initialize() override;
    void init_params() override;

    int num_inputs() const override { return 1; }
    int num_outputs() const override { return num_outputs_; }
    indicator_kind kind() const override { return kind_; }
    indicator_source source() const override { return indicator_source::python; }

    // Execute indicator on OHLCV sample
    double operator()(ohlctv_sample const& sample);

    double getLastResult() { return last_result_; }

    // Accessors for debugging and testing
    std::string const& get_class_name() const { return class_name_; }
    python_indicator_registry* get_registry() const { return registry_; }
    bool has_instance() const { return instance_ != nullptr; }
    param_list const& get_params() const override { return params_; }

private:
    std::string class_name_;
    python_indicator_registry* registry_;
    std::shared_ptr<py_indicator_instance> instance_;
    double last_result_ = 0.0;
    // Number of outputs declared by the Python class (default 1)
    int num_outputs_ = 1;
    // Kind declared by the Python class (default graph)
    indicator_kind kind_ = indicator_kind::graph;
    // Pre-allocated buffer for multi-output compute_sample returns
    std::vector<float> multi_output_buffer_;
    // Maps param index to Python attribute name for set_parameter calls
    // (params_ stores GUI labels, but Python needs the actual attribute name)
    std::vector<std::string> python_attr_names_;
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
     * Release the GIL after startup initialization is complete.
     * Must be called from the main thread after all modules are loaded
     * and indicators are registered, so that worker threads can acquire the GIL.
     */
    void release_gil();

    /**
     * Re-acquire the GIL on the main thread (for shutdown cleanup).
     */
    void reacquire_gil();

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
    std::wstring python_home_;              // Persistent storage for Py_SetPythonHome
    void* saved_thread_state_ = nullptr;    // For PyEval_SaveThread/RestoreThread
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
     * Call Python's compute_sample and return the raw result object.
     * The caller owns the reference (must Py_DECREF).
     * Supports single float, int, bool, or list/tuple of floats.
     */
    void* compute_sample_raw(
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
