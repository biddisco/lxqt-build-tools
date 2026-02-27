/**
 * Python Plugin Implementation
 *
 * Implements runtime loading and execution of Python-based indicators.
 *
 * Note: This file requires Python development headers and embeds the Python interpreter.
 * At compile time without Python, this provides no-op implementations.
 */

#include <filesystem>

#ifdef GROX_PYTHON_ENABLED
# define PY_SSIZE_T_CLEAN
# include <Python.h>
#endif

#include "indicators/python_plugin.hpp"

namespace fs = std::filesystem;

namespace indicators { namespace python {

  // ============================================================================
  // Registry Implementation
  // ============================================================================

  python_indicator_registry::~python_indicator_registry()
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] registry dtor initialized={}", initialized_);
    if (initialized_) { shutdown(); }
  }

  bool python_indicator_registry::initialize()
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] initialize called initialized={}", initialized_);
    if (initialized_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] initialize skipped (already initialized)");
      return true;
    }

#ifdef GROX_PYTHON_ENABLED
    GROX_PYPLUGIN_TRACE("[python_plugin] calling Py_Initialize()");
    Py_Initialize();
    if (!Py_IsInitialized())
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] Py_Initialize failed");
      return false;
    }
    GROX_PYPLUGIN_TRACE("[python_plugin] Python interpreter initialized");
#else
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] Python support not compiled in (GROX_PYTHON_ENABLED not defined)");
#endif

    initialized_ = true;
    GROX_PYPLUGIN_TRACE("[python_plugin] initialize complete initialized={}", initialized_);

    return true;
  }

  void python_indicator_registry::shutdown()
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] shutdown called initialized={}", initialized_);
    if (!initialized_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] shutdown skipped (not initialized)");
      return;
    }

#ifdef GROX_PYTHON_ENABLED
    // Decrement all cached Python object refcounts
    for (auto& [name, py_obj] : registered_classes_)
    {
      if (py_obj) { Py_DECREF(static_cast<PyObject*>(py_obj)); }
    }
#endif

    registered_classes_.clear();

#ifdef GROX_PYTHON_ENABLED
    GROX_PYPLUGIN_TRACE("[python_plugin] calling Py_Finalize()");
    Py_Finalize();
#endif

    initialized_ = false;
    GROX_PYPLUGIN_TRACE("[python_plugin] shutdown complete initialized={}", initialized_);
  }

  bool python_indicator_registry::load_module(fs::path const& module_path)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] load_module path={}", module_path.string());
    if (!fs::exists(module_path))
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] load_module path missing path={}", module_path.string());
      return false;
    }

#ifdef GROX_PYTHON_ENABLED
    if (!initialized_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] load_module called before initialize()");
      return false;
    }

    // Extract module name (filename without extension)
    std::string module_name = module_path.stem().string();
    std::string module_dir = module_path.parent_path().string();

    GROX_PYPLUGIN_TRACE("[python_plugin] loading module={} from dir={}", module_name, module_dir);

    // Add module directory to Python path
    PyObject* sys_path = PySys_GetObject("path");
    PyObject* py_dir = PyUnicode_FromString(module_dir.c_str());
    PyList_Append(sys_path, py_dir);
    Py_DECREF(py_dir);

    // Import the module
    PyObject* py_module_name = PyUnicode_FromString(module_name.c_str());
    PyObject* py_module = PyImport_Import(py_module_name);
    Py_DECREF(py_module_name);

    if (!py_module)
    {
      PyErr_Print();
      GROX_PYPLUGIN_TRACE("[python_plugin] failed to import module={}", module_name);
      return false;
    }

    GROX_PYPLUGIN_TRACE("[python_plugin] imported module={}", module_name);

    // Get module dictionary to scan for indicator classes
    PyObject* module_dict = PyModule_GetDict(py_module);

    // Scan for classes that look like indicators (have compute_sample method)
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(module_dict, &pos, &key, &value))
    {
      // Skip non-class objects and builtins
      if (!PyType_Check(value)) continue;

      char const* class_name_cstr = PyUnicode_AsUTF8(key);
      if (!class_name_cstr) continue;

      std::string class_name(class_name_cstr);

      // Skip private/builtin classes
      if (class_name[0] == '_') continue;

      // Check if class has compute_sample method
      if (PyObject_HasAttrString(value, "compute_sample"))
      {
        GROX_PYPLUGIN_TRACE("[python_plugin] found indicator class={}", class_name);

        // Store the class object (increment refcount)
        Py_INCREF(value);
        registered_classes_[class_name] = value;
      }
    }

    Py_DECREF(py_module);

    GROX_PYPLUGIN_TRACE("[python_plugin] load_module success path={}", module_path.string());
    return true;
#else
    // Stub implementation when Python is not available
    std::string class_name = module_path.stem().string();
    registered_classes_[class_name] = nullptr;
    GROX_PYPLUGIN_TRACE("[python_plugin] load_module registered stub class={} path={}", class_name,
        module_path.string());
    return true;
#endif
  }

  void* python_indicator_registry::get_indicator_class(std::string const& class_name)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] get_indicator_class name={}", class_name);
    auto it = registered_classes_.find(class_name);
    if (it != registered_classes_.end())
    {
      GROX_PYPLUGIN_TRACE(
          "[python_plugin] get_indicator_class hit name={} ptr={}", class_name, it->second);
      return it->second;
    }
    GROX_PYPLUGIN_TRACE("[python_plugin] get_indicator_class miss name={}", class_name);
    return nullptr;
  }

  std::shared_ptr<py_indicator_instance> python_indicator_registry::create_instance(
      std::string const& class_name)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] create_instance name={}", class_name);
    auto* py_class = get_indicator_class(class_name);
    if (!py_class)
    {
      GROX_PYPLUGIN_TRACE(
          "[python_plugin] create_instance failed (class not found) name={}", class_name);
      return nullptr;
    }

#ifdef GROX_PYTHON_ENABLED
    // Call the class to create an instance (equivalent to MyClass())
    PyObject* py_instance = PyObject_CallObject(static_cast<PyObject*>(py_class), nullptr);
    if (!py_instance)
    {
      PyErr_Print();
      GROX_PYPLUGIN_TRACE(
          "[python_plugin] create_instance failed to instantiate class={}", class_name);
      return nullptr;
    }

    auto instance = std::make_shared<py_indicator_instance>(py_instance);
    GROX_PYPLUGIN_TRACE("[python_plugin] create_instance success name={} instance_ptr={}",
        class_name, static_cast<void*>(instance.get()));
    return instance;
#else
    // Stub implementation
    auto instance = std::make_shared<py_indicator_instance>(nullptr);
    GROX_PYPLUGIN_TRACE("[python_plugin] create_instance stub name={} instance_ptr={}", class_name,
        static_cast<void*>(instance.get()));
    return instance;
#endif
  }

  std::size_t python_indicator_registry::load_indicators_from_directory(fs::path const& directory)
  {
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] load_indicators_from_directory dir={}", directory.string());
    if (!fs::is_directory(directory))
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] not a directory dir={}", directory.string());
      return 0;
    }

    std::size_t count = 0;

    // Find all .py files in the directory
    for (auto const& entry : fs::directory_iterator(directory))
    {
      if (!entry.is_regular_file()) continue;

      auto const& path = entry.path();
      if (path.extension() != ".py") continue;

      // Skip __pycache__ and similar
      if (path.filename().string()[0] == '_') continue;

      if (load_module(path)) { count++; }
    }

    GROX_PYPLUGIN_TRACE("[python_plugin] load_indicators_from_directory done dir={} count={}",
        directory.string(), count);

    return count;
  }

  std::vector<std::string> python_indicator_registry::get_available_indicators() const
  {
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] get_available_indicators count={}", registered_classes_.size());
    std::vector<std::string> result;
    for (auto const& [name, _] : registered_classes_) { result.push_back(name); }
    return result;
  }

  // ============================================================================
  // Instance Implementation
  // ============================================================================

  py_indicator_instance::py_indicator_instance(void* py_object)
    : py_object_(py_object)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] py_indicator_instance ctor py_object={}", py_object_);
  }

  py_indicator_instance::~py_indicator_instance()
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] py_indicator_instance dtor py_object={}", py_object_);
#ifdef GROX_PYTHON_ENABLED
    if (py_object_) { Py_DECREF(static_cast<PyObject*>(py_object_)); }
#endif
    py_object_ = nullptr;
  }

  double py_indicator_instance::compute_sample(
      double open, double high, double low, double close, double volume, uint64_t time)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] compute_sample called open={} high={} low={} close={} "
                        "volume={} time={} py_object={}",
        open, high, low, close, volume, time, py_object_);

    if (!py_object_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] compute_sample returning default (null py_object)");
      return 0.0;
    }

#ifdef GROX_PYTHON_ENABLED
    // Create Python dict with OHLCV data
    PyObject* ohlcv_dict = PyDict_New();
    PyDict_SetItemString(ohlcv_dict, "open", PyFloat_FromDouble(open));
    PyDict_SetItemString(ohlcv_dict, "high", PyFloat_FromDouble(high));
    PyDict_SetItemString(ohlcv_dict, "low", PyFloat_FromDouble(low));
    PyDict_SetItemString(ohlcv_dict, "close", PyFloat_FromDouble(close));
    PyDict_SetItemString(ohlcv_dict, "volume", PyFloat_FromDouble(volume));
    PyDict_SetItemString(ohlcv_dict, "time", PyLong_FromUnsignedLongLong(time));

    // Call compute_sample(ohlcv_dict)
    PyObject* result =
        PyObject_CallMethod(static_cast<PyObject*>(py_object_), "compute_sample", "O", ohlcv_dict);

    Py_DECREF(ohlcv_dict);

    if (!result)
    {
      PyErr_Print();
      GROX_PYPLUGIN_TRACE("[python_plugin] compute_sample Python call failed");
      return 0.0;
    }

    double value = PyFloat_AsDouble(result);
    Py_DECREF(result);

    GROX_PYPLUGIN_TRACE("[python_plugin] compute_sample result={}", value);
    return value;
#else
    GROX_PYPLUGIN_TRACE("[python_plugin] compute_sample returning stub default value");
    return 0.0;
#endif
  }

  bool py_indicator_instance::set_parameter(std::string const& name, double value)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<double> name={} value={} py_object={}", name,
        value, py_object_);
    // In full implementation:
    // 1. Use PyObject_SetAttrString to set attribute
    // 2. Handle exception on failure

    bool const ok = py_object_ != nullptr;
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<double> result={}", ok);
    return ok;
  }

  bool py_indicator_instance::set_parameter(std::string const& name, int value)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<int> name={} value={} py_object={}", name,
        value, py_object_);
    bool const ok = py_object_ != nullptr;
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<int> result={}", ok);
    return ok;
  }

  bool py_indicator_instance::set_parameter(std::string const& name, std::string const& value)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<string> name={} value={} py_object={}", name,
        value, py_object_);
    bool const ok = py_object_ != nullptr;
    GROX_PYPLUGIN_TRACE("[python_plugin] set_parameter<string> result={}", ok);
    return ok;
  }

  double py_indicator_instance::get_double_parameter(std::string const& name)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] get_double_parameter name={} (stub)", name);
    return 0.0;
  }

  int py_indicator_instance::get_int_parameter(std::string const& name)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] get_int_parameter name={} (stub)", name);
    return 0;
  }

  std::string py_indicator_instance::get_string_parameter(std::string const& name)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] get_string_parameter name={} (stub)", name);
    return "";
  }

  // ============================================================================
  // Python Indicator Wrapper Implementation
  // ============================================================================

  python_indicator_wrapper::python_indicator_wrapper()
    : indicator_base("Python Indicator", "Python-based indicator", {overlay_type::price})
    , class_name_("")
    , registry_(nullptr)
    , instance_(nullptr)
    , last_result_(0.0)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] python_indicator_wrapper default ctor");
  }

  python_indicator_wrapper::python_indicator_wrapper(std::string const& name,
      std::string const& description, std::string const& class_name,
      python_indicator_registry* registry)
    : indicator_base(name, description, {overlay_type::price})
    , class_name_(class_name)
    , registry_(registry)
    , instance_(nullptr)
    , last_result_(0.0)
  {
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] python_indicator_wrapper ctor name={} class_name={}", name, class_name);
  }

  void python_indicator_wrapper::initialize()
  {
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] python_indicator_wrapper::initialize class_name={}", class_name_);
    if (registry_) { instance_ = registry_->create_instance(class_name_); }
  }

  void python_indicator_wrapper::init_params()
  {
    GROX_PYPLUGIN_TRACE(
        "[python_plugin] python_indicator_wrapper::init_params class_name={}", class_name_);
    // Parameters would be populated from Python class metadata
    // For now, use default empty parameters
    params_ = {};
  }

  double python_indicator_wrapper::operator()(ohlctv_sample const& sample)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] python_indicator_wrapper::operator() called");

    if (!instance_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] operator() no instance, returning 0.0");
      return 0.0;
    }

    last_result_ = instance_->compute_sample(
        sample.open, sample.high, sample.low, sample.close, sample.volume, sample.time);

    return last_result_;
  }

  // ============================================================================
  // Registry Registration with Main Indicator Registry
  // ============================================================================

  std::size_t python_indicator_registry::register_with_main_registry(
      indicators::indicator_registry& main_registry)
  {
    GROX_PYPLUGIN_TRACE("[python_plugin] register_with_main_registry called classes_count={}",
        registered_classes_.size());

    std::size_t count = 0;

    // In a full implementation, this would extract metadata from Python classes
    // For now, we'll create wrappers for each registered class with placeholder metadata

    for (auto const& [class_name, py_class_ptr] : registered_classes_)
    {
      GROX_PYPLUGIN_TRACE("[python_plugin] registering wrapper for class={}", class_name);

      // Create wrapper with metadata extracted from Python class
      // TODO: Parse actual metadata from Python class attributes (name, description, category)
      auto wrapper = std::make_shared<python_indicator_wrapper>(class_name,    // name
          "Python-based indicator: " + class_name,                             // description
          class_name,                                                          // Python class name
          this                                                                 // registry reference
      );

      wrapper->init_params();

      // Register with main registry
      main_registry.register_indicator(wrapper);

      GROX_PYPLUGIN_TRACE("[python_plugin] wrapper registered class={}", class_name);
      count++;
    }

    GROX_PYPLUGIN_TRACE("[python_plugin] register_with_main_registry complete count={}", count);
    return count;
  }

}}    // namespace indicators::python
