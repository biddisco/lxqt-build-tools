/**
 * Python Plugin Implementation
 *
 * Implements runtime loading and execution of Python-based indicators.
 *
 * Note: This file requires Python development headers and embeds the Python interpreter.
 * At compile time without Python, this provides no-op implementations.
 */

#include <algorithm>
#include <dlfcn.h>
#include <filesystem>
//
#define PY_SSIZE_T_CLEAN
#include <Python.h>
//
#include "debug/logging.hpp"
#include "grox/config-defines.hpp"
#include "grox/config-python.hpp"
#include "indicators/python_plugin.hpp"

namespace fs = std::filesystem;

namespace indicators { namespace python {

  // RAII guard to acquire/release Python GIL when calling Python C API from C++ threads
  class PyGILGuard
  {
public:
    PyGILGuard()
      : state_(PyGILState_Ensure())
    {
    }
    ~PyGILGuard() { PyGILState_Release(state_); }
    // Prevent copying
    PyGILGuard(PyGILGuard const&) = delete;
    PyGILGuard& operator=(PyGILGuard const&) = delete;

private:
    PyGILState_STATE state_;
  };

  // Helper function to normalize parameter names for Python attribute lookup
  // Converts "Price Type" -> "price_type", "Period" -> "period", etc.
  static std::string normalize_param_name(std::string const& name)
  {
    std::string result = name;
    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    // Replace spaces with underscores
    std::replace(result.begin(), result.end(), ' ', '_');
    return result;
  }

  static std::string ohlc_mode_to_price_type(ohlc_modes mode)
  {
    switch (mode)
    {
    case ohlc_modes::open: return "open";
    case ohlc_modes::close: return "close";
    case ohlc_modes::mid_open_close: return "mid_open_close";
    case ohlc_modes::high: return "high";
    case ohlc_modes::low: return "low";
    case ohlc_modes::mid_high_low: return "mid_high_low";
    case ohlc_modes::volume: return "volume";
    case ohlc_modes::value: return "value";
    default: return "close";
    }
  }

  static ohlc_modes price_type_to_ohlc_mode(std::string const& value)
  {
    if (value == "open") return ohlc_modes::open;
    if (value == "close") return ohlc_modes::close;
    if (value == "mid_open_close") return ohlc_modes::mid_open_close;
    if (value == "high") return ohlc_modes::high;
    if (value == "low") return ohlc_modes::low;
    if (value == "mid_high_low") return ohlc_modes::mid_high_low;
    if (value == "volume") return ohlc_modes::volume;
    if (value == "value") return ohlc_modes::value;
    return ohlc_modes::close;
  }

  // ============================================================================
  // Registry Implementation
  // ============================================================================

  python_indicator_registry::~python_indicator_registry()
  {
    // NOTE: No logging in destructor - logging system may be destroyed during static teardown
    if (initialized_) { shutdown(); }
  }

  bool python_indicator_registry::initialize()
  {
    GROX_LOG_TRACE(
        py_plug_log, "{:>20} initialize called initialized={}", "registry", initialized_);
    if (initialized_)
    {
      GROX_LOG_TRACE(py_plug_log, "{:>20} initialize skipped (already initialized)", "registry");
      return true;
    }

    // Determine Python home for embedded interpreter
    GROX_LOG_TRACE(py_plug_log, "{:>20} detecting Python home", "registry");
    bool have_python_home = false;

    char const* python_home_env = std::getenv("PYTHONHOME");
    if (python_home_env)
    {
      python_home_ = std::wstring(python_home_env, python_home_env + strlen(python_home_env));
      have_python_home = true;
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} Using PYTHONHOME from environment: {}", "registry", python_home_env);
    }
#ifdef GROX_PYTHON_BASE_PREFIX
    if (!have_python_home)
    {
      std::string const configured_home = GROX_PYTHON_BASE_PREFIX;
      if (!configured_home.empty())
      {
        python_home_ = std::wstring(configured_home.begin(), configured_home.end());
        have_python_home = true;
        GROX_LOG_DEBUG(
            py_plug_log, "{:>20} Using CMake Python home: {}", "registry", configured_home);
      }
    }
#endif
    if (!have_python_home)
    {
      // Auto-detect Python home by finding libpython shared library
      // Use dladdr to find where Python symbols are loaded from
      Dl_info dl_info;
      // Use Py_GetVersion address to find the library
      if (dladdr((void*) Py_GetVersion, &dl_info) && dl_info.dli_fname)
      {
        std::string lib_path(dl_info.dli_fname);
        GROX_LOG_DEBUG(py_plug_log, "{:>20} Found Python library: {}", "registry", lib_path);

        // Extract PYTHONHOME from library path
        // Typical: /path/to/python/lib/libpython3.13.so -> /path/to/python
        size_t lib_pos = lib_path.rfind("/lib/");
        if (lib_pos != std::string::npos)
        {
          std::string home = lib_path.substr(0, lib_pos);
          python_home_ = std::wstring(home.begin(), home.end());
          GROX_LOG_DEBUG(py_plug_log, "{:>20} Auto-detected PYTHONHOME: {}", "registry", home);
        }
        else
        {
          GROX_LOG_WARN(
              py_plug_log, "{:>20} Could not derive PYTHONHOME from library path", "registry");
        }
      }
      else
      {
        GROX_LOG_WARN(py_plug_log, "{:>20} Could not find Python library location", "registry");
      }
    }

    // Initialize Python using PyConfig API (replaces deprecated Py_SetPythonHome/Py_Initialize)
    GROX_LOG_TRACE(py_plug_log, "{:>20} initializing Python interpreter", "registry");
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    if (!python_home_.empty())
    {
      PyStatus status = PyConfig_SetString(&config, &config.home, python_home_.c_str());
      if (PyStatus_Exception(status))
      {
        GROX_LOG_ERROR(
            py_plug_log, "{:>20} PyConfig_SetString failed: {}", "registry", status.err_msg);
        PyConfig_Clear(&config);
        return false;
      }
    }

    PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    if (PyStatus_Exception(status))
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} Py_InitializeFromConfig failed", "registry");
      return false;
    }

    if (!Py_IsInitialized())
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} Python initialization check failed", "registry");
      return false;
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} Python interpreter initialized", "registry");

    initialized_ = true;
    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} initialize complete initialized={}", "registry", initialized_);

    return true;
  }

  void python_indicator_registry::release_gil()
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} releasing GIL (main thread)", "registry");
    saved_thread_state_ = static_cast<void*>(PyEval_SaveThread());
  }

  void python_indicator_registry::reacquire_gil()
  {
    if (saved_thread_state_)
    {
      GROX_LOG_DEBUG(py_plug_log, "{:>20} re-acquiring GIL (main thread)", "registry");
      PyEval_RestoreThread(static_cast<PyThreadState*>(saved_thread_state_));
      saved_thread_state_ = nullptr;
    }
  }

  void python_indicator_registry::shutdown()
  {
    GROX_LOG_TRACE(py_plug_log, "{:>20} shutdown called initialized={}", "registry", initialized_);
    if (!initialized_)
    {
      GROX_LOG_TRACE(py_plug_log, "{:>20} shutdown skipped (not initialized)", "registry");
      return;
    }

    // Re-acquire the GIL on the main thread if it was released
    reacquire_gil();

    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} clearing registered classes (skipping Py_Finalize)", "registry");

    // Main thread now holds the GIL, safe to Py_DECREF
    for (auto& [name, py_obj] : registered_classes_)
    {
      if (py_obj) { Py_DECREF(static_cast<PyObject*>(py_obj)); }
    }

    registered_classes_.clear();
    initialized_ = false;

    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} shutdown complete (Python interpreter left alive)", "registry");
  }

  bool python_indicator_registry::load_module(fs::path const& module_path)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} load_module path={}", "registry", module_path.string());
    if (!fs::exists(module_path))
    {
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} load_module path missing path={}", "registry", module_path.string());
      return false;
    }

    if (!initialized_)
    {
      GROX_LOG_DEBUG(py_plug_log, "{:>20} load_module called before initialize()", "registry");
      return false;
    }

    // Extract module name (filename without extension)
    std::string module_name = module_path.stem().string();
    std::string module_dir = module_path.parent_path().string();

    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} loading module={} from dir={}", "registry", module_name, module_dir);

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
      GROX_LOG_DEBUG(py_plug_log, "{:>20} failed to import module={}", "registry", module_name);
      return false;
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} imported module={}", "registry", module_name);

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
        GROX_LOG_DEBUG(py_plug_log, "{:>20} found indicator class={}", "registry", class_name);

        // Store the class object (increment refcount)
        Py_INCREF(value);
        registered_classes_[class_name] = value;
      }
    }

    Py_DECREF(py_module);

    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} load_module success path={}", "registry", module_path.string());
    return true;
  }

  void* python_indicator_registry::get_indicator_class(std::string const& class_name)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_indicator_class name={}", "registry", class_name);
    auto it = registered_classes_.find(class_name);
    if (it != registered_classes_.end())
    {
      GROX_LOG_DEBUG(py_plug_log, "{:>20} get_indicator_class hit name={} ptr={}", "registry",
          class_name, it->second);
      return it->second;
    }
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_indicator_class miss name={}", "registry", class_name);
    return nullptr;
  }

  std::shared_ptr<py_indicator_instance> python_indicator_registry::create_instance(
      std::string const& class_name)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} create_instance name={}", "registry", class_name);

    // Acquire GIL for all Python C API calls
    PyGILGuard gil;

    auto* py_class = get_indicator_class(class_name);
    if (!py_class)
    {
      GROX_LOG_DEBUG(py_plug_log, "{:>20} create_instance failed (class not found) name={}",
          "registry", class_name);
      return nullptr;
    }

    GROX_LOG_TRACE(py_plug_log, "{:>20} class object ptr={} type_check={}", "registry",
        static_cast<void*>(py_class),
        PyType_Check(static_cast<PyObject*>(py_class)) ? "YES" : "NO");

    // Verify the class is callable
    if (!PyCallable_Check(static_cast<PyObject*>(py_class)))
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} class is not callable class_name={}", "registry", class_name);
      return nullptr;
    }

    GROX_LOG_TRACE(py_plug_log, "{:>20} calling class to instantiate", "registry");

    // Call the class to create an instance (equivalent to MyClass())
    PyObject* py_instance = PyObject_CallObject(static_cast<PyObject*>(py_class), nullptr);

    GROX_LOG_TRACE(py_plug_log, "{:>20} PyObject_CallObject returned ptr={}", "registry",
        static_cast<void*>(py_instance));

    if (!py_instance)
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} create_instance failed to instantiate class={}",
          "registry", class_name);
      PyErr_Print();
      return nullptr;
    }

    // Verify the instance has compute_sample method
    if (!PyObject_HasAttrString(py_instance, "compute_sample"))
    {
      GROX_LOG_ERROR(py_plug_log,
          "{:>20} created instance missing compute_sample method class_name={}", "registry",
          class_name);
      Py_DECREF(py_instance);
      return nullptr;
    }

    auto instance = std::make_shared<py_indicator_instance>(py_instance);
    GROX_LOG_DEBUG(py_plug_log, "{:>20} create_instance success name={} instance_ptr={}",
        "registry", class_name, static_cast<void*>(instance.get()));
    return instance;
  }

  std::size_t python_indicator_registry::load_indicators_from_directory(fs::path const& directory)
  {
    GROX_LOG_TRACE(py_plug_log, "{:>20} load_indicators_from_directory dir={}", "registry",
        directory.string());
    if (!fs::is_directory(directory))
    {
      GROX_LOG_TRACE(py_plug_log, "{:>20} not a directory dir={}", "registry", directory.string());
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

    GROX_LOG_TRACE(py_plug_log, "{:>20} load_indicators_from_directory done dir={} count={}",
        "registry", directory.string(), count);

    return count;
  }

  std::vector<std::string> python_indicator_registry::get_available_indicators() const
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_available_indicators count={}", "registry",
        registered_classes_.size());
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
    GROX_LOG_DEBUG(py_plug_log, "{:>20} instance ctor py_object={}", "py_indicator", py_object_);
    // Verify object is valid Python object
    if (py_object_)
    {
      auto* obj = static_cast<PyObject*>(py_object_);
      GROX_LOG_TRACE(py_plug_log, "{:>20} py_object refcount={}", "py_indicator", Py_REFCNT(obj));
    }
  }

  py_indicator_instance::~py_indicator_instance()
  {
    // NOTE: No logging in destructor - logging system may be destroyed during static teardown
    if (py_object_)
    {
      // Must hold GIL when calling Py_DECREF
      PyGILGuard gil;
      auto* obj = static_cast<PyObject*>(py_object_);
      if (Py_REFCNT(obj) > 0) { Py_DECREF(obj); }
    }
    py_object_ = nullptr;
  }

  double py_indicator_instance::compute_sample(
      double open, double high, double low, double close, double volume, uint64_t time)
  {
    GROX_LOG_DEBUG(py_plug_log,
        "{:>20} compute_sample called open={} high={} low={} close={} "
        "volume={} time={} py_object={}",
        "py_indicator", open, high, low, close, volume, time, py_object_);

    if (!py_object_)
    {
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} compute_sample returning default (null py_object)", "py_indicator");
      return 0.0;
    }

    // Acquire GIL for all Python C API calls
    PyGILGuard gil;

    // Create Python dict with OHLCV data
    PyObject* ohlcv_dict = PyDict_New();
    if (!ohlcv_dict)
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} failed to create OHLCV dict", "py_indicator");
      return 0.0;
    }

// Helper macro to safely add items to dict (manages reference counts)
#define SAFE_DICT_SET_FLOAT(dict, key, val)                                                        \
  do {                                                                                             \
    PyObject* tmp = PyFloat_FromDouble(val);                                                       \
    if (!tmp)                                                                                      \
    {                                                                                              \
      GROX_LOG_ERROR(                                                                              \
          py_plug_log, "{:>20} failed to create float for key={}", "py_indicator", key);           \
      Py_DECREF(dict);                                                                             \
      return 0.0;                                                                                  \
    }                                                                                              \
    int ret = PyDict_SetItemString(dict, key, tmp);                                                \
    Py_DECREF(tmp);                                                                                \
    if (ret < 0)                                                                                   \
    {                                                                                              \
      GROX_LOG_ERROR(py_plug_log, "{:>20} failed to set dict key={}", "py_indicator", key);        \
      Py_DECREF(dict);                                                                             \
      return 0.0;                                                                                  \
    }                                                                                              \
  } while (0)

#define SAFE_DICT_SET_LONG(dict, key, val)                                                         \
  do {                                                                                             \
    PyObject* tmp = PyLong_FromUnsignedLongLong(val);                                              \
    if (!tmp)                                                                                      \
    {                                                                                              \
      GROX_LOG_ERROR(py_plug_log, "{:>20} failed to create long for key={}", "py_indicator", key); \
      Py_DECREF(dict);                                                                             \
      return 0.0;                                                                                  \
    }                                                                                              \
    int ret = PyDict_SetItemString(dict, key, tmp);                                                \
    Py_DECREF(tmp);                                                                                \
    if (ret < 0)                                                                                   \
    {                                                                                              \
      GROX_LOG_ERROR(py_plug_log, "{:>20} failed to set dict key={}", "py_indicator", key);        \
      Py_DECREF(dict);                                                                             \
      return 0.0;                                                                                  \
    }                                                                                              \
  } while (0)

    SAFE_DICT_SET_FLOAT(ohlcv_dict, "open", open);
    SAFE_DICT_SET_FLOAT(ohlcv_dict, "high", high);
    SAFE_DICT_SET_FLOAT(ohlcv_dict, "low", low);
    SAFE_DICT_SET_FLOAT(ohlcv_dict, "close", close);
    SAFE_DICT_SET_FLOAT(ohlcv_dict, "volume", volume);
    SAFE_DICT_SET_LONG(ohlcv_dict, "time", time);

    GROX_LOG_TRACE(py_plug_log, "{:>20} calling compute_sample on Python object", "py_indicator");

    // Call compute_sample(ohlcv_dict)
    PyObject* result =
        PyObject_CallMethod(static_cast<PyObject*>(py_object_), "compute_sample", "O", ohlcv_dict);

    Py_DECREF(ohlcv_dict);

    if (!result)
    {
      PyErr_Print();
      GROX_LOG_ERROR(py_plug_log, "{:>20} compute_sample Python call failed", "py_indicator");
      return 0.0;
    }

    if (!PyFloat_Check(result))
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} compute_sample returned non-float result", "py_indicator");
      Py_DECREF(result);
      return 0.0;
    }

    double value = PyFloat_AsDouble(result);
    Py_DECREF(result);

    GROX_LOG_DEBUG(py_plug_log, "{:>20} compute_sample result={}", "py_indicator", value);
    return value;

#undef SAFE_DICT_SET_FLOAT
#undef SAFE_DICT_SET_LONG
  }

  bool py_indicator_instance::set_parameter(std::string const& name, double value)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<double> name={} value={} py_object={}",
        "py_indicator", name, value, py_object_);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<double> failed: null py_object", "py_indicator");
      return false;
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Create Python float
    PyObject* py_value = PyFloat_FromDouble(value);
    if (!py_value)
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} set_parameter<double> failed to create Python float",
          "py_indicator");
      return false;
    }

    // Set attribute on Python object
    int result =
        PyObject_SetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str(), py_value);
    Py_DECREF(py_value);

    if (result < 0)
    {
      PyErr_Print();
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<double> failed to set attribute", "py_indicator");
      return false;
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<double> success", "py_indicator");
    return true;
  }

  bool py_indicator_instance::set_parameter(std::string const& name, int value)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<int> name={} value={} py_object={}",
        "py_indicator", name, value, py_object_);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<int> failed: null py_object", "py_indicator");
      return false;
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Create Python long
    PyObject* py_value = PyLong_FromLong(value);
    if (!py_value)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<int> failed to create Python int", "py_indicator");
      return false;
    }

    // Set attribute on Python object
    int result =
        PyObject_SetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str(), py_value);
    Py_DECREF(py_value);

    if (result < 0)
    {
      PyErr_Print();
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<int> failed to set attribute", "py_indicator");
      return false;
    }

    // Special handling for 'period' parameter - need to reinitialize the deque
    if (attr_name == "period" &&
        PyObject_HasAttrString(static_cast<PyObject*>(py_object_), "prices"))
    {
      // Call Python to rebuild the prices deque with new maxlen
      // prices = deque(prices, maxlen=period)
      PyObject* deque_module = PyImport_ImportModule("collections");
      if (deque_module)
      {
        PyObject* deque_class = PyObject_GetAttrString(deque_module, "deque");
        if (deque_class)
        {
          PyObject* old_prices =
              PyObject_GetAttrString(static_cast<PyObject*>(py_object_), "prices");
          if (old_prices)
          {
            PyObject* args = PyTuple_Pack(1, old_prices);
            PyObject* kwargs = Py_BuildValue("{s:i}", "maxlen", value);
            PyObject* new_prices = PyObject_Call(deque_class, args, kwargs);

            if (new_prices)
            {
              PyObject_SetAttrString(static_cast<PyObject*>(py_object_), "prices", new_prices);
              Py_DECREF(new_prices);
            }

            Py_XDECREF(kwargs);
            Py_XDECREF(args);
            Py_DECREF(old_prices);
          }
          Py_DECREF(deque_class);
        }
        Py_DECREF(deque_module);
      }
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<int> success", "py_indicator");
    return true;
  }

  bool py_indicator_instance::set_parameter(std::string const& name, std::string const& value)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<string> name={} value={} py_object={}",
        "py_indicator", name, value, py_object_);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<string> failed: null py_object", "py_indicator");
      return false;
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Create Python string
    PyObject* py_value = PyUnicode_FromString(value.c_str());
    if (!py_value)
    {
      GROX_LOG_ERROR(py_plug_log, "{:>20} set_parameter<string> failed to create Python string",
          "py_indicator");
      return false;
    }

    // Set attribute on Python object
    int result =
        PyObject_SetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str(), py_value);
    Py_DECREF(py_value);

    if (result < 0)
    {
      PyErr_Print();
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} set_parameter<string> failed to set attribute", "py_indicator");
      return false;
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} set_parameter<string> success", "py_indicator");
    return true;
  }

  double py_indicator_instance::get_double_parameter(std::string const& name)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_double_parameter name={}", "py_indicator", name);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} get_double_parameter failed: null py_object", "py_indicator");
      return 0.0;
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Get attribute from Python object
    PyObject* py_value =
        PyObject_GetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str());
    if (!py_value)
    {
      PyErr_Clear();    // Clear the error
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} get_double_parameter: attribute not found", "py_indicator");
      return 0.0;
    }

    double result = 0.0;
    if (PyFloat_Check(py_value)) { result = PyFloat_AsDouble(py_value); }
    else if (PyLong_Check(py_value)) { result = static_cast<double>(PyLong_AsLong(py_value)); }

    Py_DECREF(py_value);
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_double_parameter={}", "py_indicator", result);
    return result;
  }

  int py_indicator_instance::get_int_parameter(std::string const& name)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_int_parameter name={}", "py_indicator", name);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} get_int_parameter failed: null py_object", "py_indicator");
      return 0;
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Get attribute from Python object
    PyObject* py_value =
        PyObject_GetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str());
    if (!py_value)
    {
      PyErr_Clear();    // Clear the error
      GROX_LOG_DEBUG(py_plug_log, "{:>20} get_int_parameter: attribute not found", "py_indicator");
      return 0;
    }

    int result = 0;
    if (PyLong_Check(py_value)) { result = PyLong_AsLong(py_value); }
    else if (PyFloat_Check(py_value)) { result = static_cast<int>(PyFloat_AsDouble(py_value)); }

    Py_DECREF(py_value);
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_int_parameter={}", "py_indicator", result);
    return result;
  }

  std::string py_indicator_instance::get_string_parameter(std::string const& name)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_string_parameter name={}", "py_indicator", name);

    if (!py_object_)
    {
      GROX_LOG_ERROR(
          py_plug_log, "{:>20} get_string_parameter failed: null py_object", "py_indicator");
      return "";
    }

    // Convert parameter name for Python attribute lookup
    std::string attr_name = normalize_param_name(name);

    // Get attribute from Python object
    PyObject* py_value =
        PyObject_GetAttrString(static_cast<PyObject*>(py_object_), attr_name.c_str());
    if (!py_value)
    {
      PyErr_Clear();    // Clear the error
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} get_string_parameter: attribute not found", "py_indicator");
      return "";
    }

    std::string result;
    if (PyUnicode_Check(py_value))
    {
      char const* str = PyUnicode_AsUTF8(py_value);
      if (str) { result = str; }
    }

    Py_DECREF(py_value);
    GROX_LOG_DEBUG(py_plug_log, "{:>20} get_string_parameter={}", "py_indicator", result);
    return result;
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
    GROX_LOG_DEBUG(py_plug_log, "{:>20} default ctor", "py_indicator_wrapper");
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
    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} ctor name={} class_name={}", "py_indicator_wrapper", name, class_name);

    // Extract Python parameter specifications so the template has params for GUI display
    init_params();
  }

  // Custom factory create method
  // Note: Do NOT call init_params() here - the template already has params_ set
  // from construction/registration, and the copy preserves user-modified values from the GUI.
  indicators::shared_indicator python_indicator_wrapper::create(
      algorithm_base* alg, std::shared_ptr<ohlc_dataset_view> hdf5_ohlc) const
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} create method called", "py_indicator_wrapper");
    auto result = std::make_shared<python_indicator_wrapper>();
    *result = *dynamic_cast<python_indicator_wrapper*>(alg);
    result->hdf5_ohlc_ = hdf5_ohlc;

    result->initialize();
    result->create_outputs(hdf5_ohlc);
    result->register_callbacks();
    return result;
  }

  void python_indicator_wrapper::execute_from(std::uint64_t N)
  {
    if ((N == std::numeric_limits<std::uint64_t>::max()) || (N > get_input(0).dataset_->size()))
    {
      N = 0;
    }
    execute_streaming(N);
  }

  void python_indicator_wrapper::execute_continue()
  {
    std::uint64_t N = 1;
    if ((N == std::numeric_limits<std::uint64_t>::max()) || (N > get_input(0).dataset_->size()))
      N = 0;
    execute_streaming(N);
  }

  void python_indicator_wrapper::initialize()
  {
    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} initialize class_name={}", "py_indicator_wrapper", class_name_);
    if (registry_)
    {
      // Acquire GIL for create_instance + set_parameter calls
      PyGILGuard gil;

      instance_ = registry_->create_instance(class_name_);

      // Apply parameters from params_ to the Python instance
      // Skip index 0 (candle_data) - it's a C++ infrastructure param, not a Python attribute
      if (instance_ && python_attr_names_.size() > 0)
      {
        GROX_LOG_DEBUG(py_plug_log, "{:>20} applying {} parameters to Python instance",
            "py_indicator_wrapper", python_attr_names_.size());

        for (size_t i = 0; i < params_.size(); ++i)
        {
          // Skip params that have no corresponding Python attribute (e.g. candle_data at index 0)
          if (i >= python_attr_names_.size() || python_attr_names_[i].empty()) continue;

          std::string const& py_attr = python_attr_names_[i];
          std::visit(
              [this, i, &py_attr](auto&& param_variant) {
                using T = std::decay_t<decltype(param_variant.val_)>;

                if constexpr (std::is_same_v<T, int>)
                {
                  instance_->set_parameter(py_attr, param_variant.val_);
                  GROX_LOG_TRACE(py_plug_log, "{:>20} set param[{}] py_attr={} value={}",
                      "py_indicator_wrapper", i, py_attr, param_variant.val_);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                  instance_->set_parameter(py_attr, param_variant.val_);
                  GROX_LOG_TRACE(py_plug_log, "{:>20} set param[{}] py_attr={} value={}",
                      "py_indicator_wrapper", i, py_attr, param_variant.val_);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                  instance_->set_parameter(py_attr, param_variant.val_);
                  GROX_LOG_TRACE(py_plug_log, "{:>20} set param[{}] py_attr={} value={}",
                      "py_indicator_wrapper", i, py_attr, param_variant.val_);
                }
                else if constexpr (std::is_same_v<T, ohlc_modes>)
                {
                  std::string value = ohlc_mode_to_price_type(param_variant.val_);
                  instance_->set_parameter(py_attr, value);
                  GROX_LOG_TRACE(py_plug_log, "{:>20} set param[{}] py_attr={} value={}",
                      "py_indicator_wrapper", i, py_attr, value);
                }
                // candle_data and other C++-only types are silently skipped
              },
              params_[i]);
        }
      }
    }
  }

  void python_indicator_wrapper::init_params()
  {
    GROX_LOG_DEBUG(
        py_plug_log, "{:>20} init_params class_name={}", "py_indicator_wrapper", class_name_);

    // Extract parameters from Python class by creating a temporary instance
    if (!registry_)
    {
      GROX_LOG_WARN(py_plug_log, "{:>20} init_params: no registry", "py_indicator_wrapper");
      params_ = {};
      return;
    }

    // Acquire GIL BEFORE creating temp instance so that:
    // 1. create_instance()'s nested GILGuard is a safe no-op
    // 2. All subsequent Python C API calls are protected
    // 3. temp_instance is destroyed BEFORE GIL is released (C++ destruction order)
    PyGILGuard gil;

    auto temp_instance = registry_->create_instance(class_name_);
    if (!temp_instance)
    {
      GROX_LOG_WARN(py_plug_log, "{:>20} init_params: failed to create temp instance",
          "py_indicator_wrapper");
      params_ = {};
      return;
    }

    // Call init_params() on the Python instance to set defaults
    PyObject* py_obj = static_cast<PyObject*>(temp_instance->get_py_object());
    if (PyObject_HasAttrString(py_obj, "init_params"))
    {
      PyObject* result = PyObject_CallMethod(py_obj, "init_params", nullptr);
      if (result) { Py_DECREF(result); }
      else { PyErr_Print(); }
    }

    // Extract parameters from Python instance
    params_.clear();
    python_attr_names_.clear();

    // First param MUST be candle_data so that connect_candle_input_datasets()
    // can find the input dataset (same convention as all C++ indicators)
    params_.push_back(param<candle_data>{"Samples", {ohlc_data_resolutions::minute15}});
    python_attr_names_.push_back("");    // No Python attribute for candle_data

    // Try to get the Python class to access param_specs
    PyObject* py_class = PyObject_GetAttrString(py_obj, "__class__");
    if (!py_class)
    {
      GROX_LOG_WARN(py_plug_log, "{:>20} failed to get Python class", "py_indicator_wrapper");
      Py_XDECREF(py_class);
      return;
    }

    // Check if the class has param_specs
    if (PyObject_HasAttrString(py_class, "param_specs"))
    {
      PyObject* param_specs = PyObject_GetAttrString(py_class, "param_specs");
      if (param_specs && PyList_Check(param_specs))
      {
        GROX_LOG_DEBUG(py_plug_log, "{:>20} reading param_specs list", "py_indicator_wrapper");

        // Iterate through param_specs tuples
        for (Py_ssize_t i = 0; i < PyList_Size(param_specs); ++i)
        {
          PyObject* spec = PyList_GetItem(param_specs, i);
          if (!spec || !PyTuple_Check(spec) || PyTuple_Size(spec) != 3)
          {
            GROX_LOG_WARN(
                py_plug_log, "{:>20} invalid param_spec at index {}", "py_indicator_wrapper", i);
            continue;
          }

          // Extract tuple: (attr_name, gui_label, type_name)
          PyObject* attr_name_obj = PyTuple_GetItem(spec, 0);
          PyObject* gui_label_obj = PyTuple_GetItem(spec, 1);
          PyObject* type_name_obj = PyTuple_GetItem(spec, 2);

          if (!PyUnicode_Check(attr_name_obj) || !PyUnicode_Check(gui_label_obj) ||
              !PyUnicode_Check(type_name_obj))
          {
            GROX_LOG_WARN(py_plug_log, "{:>20} param_spec tuple contains non-string at index {}",
                "py_indicator_wrapper", i);
            continue;
          }

          char const* attr_name = PyUnicode_AsUTF8(attr_name_obj);
          char const* gui_label = PyUnicode_AsUTF8(gui_label_obj);
          char const* type_name = PyUnicode_AsUTF8(type_name_obj);

          GROX_LOG_DEBUG(py_plug_log, "{:>20} processing param_spec: attr={} label={} type={}",
              "py_indicator_wrapper", attr_name, gui_label, type_name);

          // Get the current value from the Python instance
          if (!PyObject_HasAttrString(py_obj, attr_name))
          {
            GROX_LOG_WARN(py_plug_log, "{:>20} Python object missing attribute '{}'",
                "py_indicator_wrapper", attr_name);
            continue;
          }

          PyObject* value_obj = PyObject_GetAttrString(py_obj, attr_name);
          if (!value_obj)
          {
            GROX_LOG_WARN(py_plug_log, "{:>20} failed to get attribute '{}'",
                "py_indicator_wrapper", attr_name);
            continue;
          }

          // Create param<T> based on type_name
          std::string type_str(type_name);
          if (type_str == "int")
          {
            if (PyLong_Check(value_obj))
            {
              int int_value = PyLong_AsLong(value_obj);
              params_.push_back(param<int>{gui_label, int_value});
              python_attr_names_.push_back(attr_name);
              GROX_LOG_DEBUG(py_plug_log, "{:>20} extracted int param: {}={}",
                  "py_indicator_wrapper", attr_name, int_value);
            }
            else
            {
              GROX_LOG_WARN(
                  py_plug_log, "{:>20} param '{}' not an int", "py_indicator_wrapper", attr_name);
            }
          }
          else if (type_str == "double" || type_str == "float")
          {
            if (PyFloat_Check(value_obj) || PyLong_Check(value_obj))
            {
              double double_value = PyFloat_AsDouble(value_obj);
              params_.push_back(param<double>{gui_label, double_value});
              python_attr_names_.push_back(attr_name);
              GROX_LOG_DEBUG(py_plug_log, "{:>20} extracted double param: {}={}",
                  "py_indicator_wrapper", attr_name, double_value);
            }
            else
            {
              GROX_LOG_WARN(py_plug_log, "{:>20} param '{}' not a float/double",
                  "py_indicator_wrapper", attr_name);
            }
          }
          else if (type_str == "string" || type_str == "str")
          {
            if (PyUnicode_Check(value_obj))
            {
              char const* str_value = PyUnicode_AsUTF8(value_obj);
              if (str_value)
              {
                params_.push_back(param<std::string>{gui_label, std::string(str_value)});
                python_attr_names_.push_back(attr_name);
                GROX_LOG_DEBUG(py_plug_log, "{:>20} extracted string param: {}={}",
                    "py_indicator_wrapper", attr_name, str_value);
              }
            }
            else
            {
              GROX_LOG_WARN(
                  py_plug_log, "{:>20} param '{}' not a string", "py_indicator_wrapper", attr_name);
            }
          }
          else if (type_str == "ohlc_modes" || type_str == "mode")
          {
            if (PyUnicode_Check(value_obj))
            {
              char const* str_value = PyUnicode_AsUTF8(value_obj);
              if (str_value)
              {
                ohlc_modes mode_value = price_type_to_ohlc_mode(str_value);
                params_.push_back(param<ohlc_modes>{gui_label, mode_value});
                python_attr_names_.push_back(attr_name);
                GROX_LOG_DEBUG(py_plug_log, "{:>20} extracted ohlc_modes param: {}={}",
                    "py_indicator_wrapper", attr_name, str_value);
              }
            }
            else
            {
              GROX_LOG_WARN(py_plug_log, "{:>20} param '{}' not a string for ohlc_modes",
                  "py_indicator_wrapper", attr_name);
            }
          }
          else if (type_str == "bool")
          {
            if (PyBool_Check(value_obj))
            {
              bool bool_value = PyObject_IsTrue(value_obj);
              params_.push_back(param<bool>{gui_label, bool_value});
              python_attr_names_.push_back(attr_name);
              GROX_LOG_DEBUG(py_plug_log, "{:>20} extracted bool param: {}={}",
                  "py_indicator_wrapper", attr_name, bool_value);
            }
            else
            {
              GROX_LOG_WARN(
                  py_plug_log, "{:>20} param '{}' not a bool", "py_indicator_wrapper", attr_name);
            }
          }
          else
          {
            GROX_LOG_WARN(py_plug_log, "{:>20} unknown parameter type '{}' for attr '{}'",
                "py_indicator_wrapper", type_name, attr_name);
          }

          Py_DECREF(value_obj);
        }

        Py_DECREF(param_specs);
      }
      else
      {
        GROX_LOG_WARN(py_plug_log, "{:>20} param_specs is not a list", "py_indicator_wrapper");
      }
    }
    else
    {
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} no param_specs found in Python class", "py_indicator_wrapper");
    }

    Py_DECREF(py_class);

    GROX_LOG_DEBUG(py_plug_log, "{:>20} init_params extracted {} parameters",
        "py_indicator_wrapper", params_.size());
  }

  double python_indicator_wrapper::operator()(ohlctv_sample const& sample)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} operator() called", "py_indicator_wrapper");

    if (!instance_)
    {
      GROX_LOG_DEBUG(py_plug_log, "{:>20} no instance, returning 0.0", "py_indicator_wrapper");
      return 0.0;
    }

    last_result_ = instance_->compute_sample(
        sample.open, sample.high, sample.low, sample.close, sample.volume, sample.time);

    return last_result_;
  }

  sample_result python_indicator_wrapper::process_sample(market_sample const& sample)
  {
    auto const& ohlc = std::get<ohlctv_sample>(sample);
    return operator()(ohlc);
  }

  // ============================================================================
  // Registry Registration with Main Indicator Registry
  // ============================================================================

  std::size_t python_indicator_registry::register_with_main_registry(
      indicators::indicator_registry& main_registry)
  {
    GROX_LOG_DEBUG(py_plug_log, "{:>20} register_with_main_registry called classes_count={}",
        "python_registry", registered_classes_.size());

    std::size_t count = 0;

    // In a full implementation, this would extract metadata from Python classes
    // For now, we'll create wrappers for each registered class with placeholder metadata

    for (auto const& [class_name, py_class_ptr] : registered_classes_)
    {
      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} registering wrapper for class={}", "python_registry", class_name);

      // Constructor already calls init_params() to extract Python parameter specs
      auto wrapper = std::make_shared<python_indicator_wrapper>(class_name,    // name
          "Python-based indicator: " + class_name,                             // description
          class_name,                                                          // Python class name
          this                                                                 // registry reference
      );

      // Register with main registry
      main_registry.register_indicator(wrapper);

      GROX_LOG_DEBUG(
          py_plug_log, "{:>20} wrapper registered class={}", "python_registry", class_name);
      count++;
    }

    GROX_LOG_DEBUG(py_plug_log, "{:>20} register_with_main_registry complete count={}",
        "python_registry", count);
    return count;
  }

}}    // namespace indicators::python
