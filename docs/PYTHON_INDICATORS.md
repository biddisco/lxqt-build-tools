# Python Indicators

Grox supports Python-based indicators alongside compiled C++ plugins.
Python indicators are discovered at startup, registered with the
unified `indicator_registry` (partitioned by `kind()`), and executed
on pika worker threads via the Python C API.

## Why Python indicators?

- **Rapid prototyping** — no recompile required
- **Python libraries** — NumPy, SciPy, pandas, etc.
- **Low barrier** — accessible to non-C++ developers
- **Same GUI** — appear alongside C++ indicators, distinguished by
  `source() == indicator_source::python`

## Architecture

```
Grox Application (Qt/C++ process)
│
├── main.cpp: startup
│   ├── indicator_registry::getInstance()
│   ├── plugin_loader: load .so files → register_indicator()
│   ├── python_indicator_registry::instance().initialize()
│   ├── python_indicator_registry::load_indicators_from_directory()
│   ├── python_indicator_registry::register_with_main_registry()
│   └── python_indicator_registry::release_gil()
│
└── indicator_registry (singleton)
    └── indicators_by_kind_[graph | strategy | orderbook]
        ├── C++ indicators    (source() == cpp)
        └── Python wrappers   (source() == python)
            └── python_indicator_wrapper
                └── py_indicator_instance
                    └── Python object
                        └── compute_sample(ohlcv_dict)
```

### Key components

| Component                       | File                          | Purpose                                      |
|---------------------------------|-------------------------------|----------------------------------------------|
| `indicator_registry`            | `indicator_registry.hpp/cpp`  | Unified singleton, partitioned by `kind()`   |
| `algorithm_base`                | `algorithm_base.hpp`          | `kind()`, `source()`, `num_outputs()` virtuals |
| `indicator_types`               | `indicator_types.hpp`         | `indicator_kind`, `indicator_source` enums   |
| `python_indicator_registry`     | `python_plugin.hpp/cpp`       | Python interpreter, module discovery, GIL    |
| `python_indicator_wrapper`      | `python_plugin.hpp/cpp`       | Adapts Python class to `indicator_base`      |
| `py_indicator_instance`         | `python_plugin.hpp/cpp`       | Wraps a single Python object, calls methods  |
| Example indicators              | `python/indicator_example.py` | SMA, EMA, WMA                                 |
| Quick start template            | `python/quick_start.py`       | Minimal, parameterized, advanced examples    |

### GIL management

The Python GIL is managed as follows:

1. **Main thread** at startup: `python_indicator_registry::initialize()`
   calls `Py_InitializeFromConfig`. All module loading and instance
   creation happens with the GIL held. After registration,
   `release_gil()` calls `PyEval_SaveThread()` so pika worker threads
   can acquire it.
2. **Pika worker threads**: `PyGILGuard` (RAII wrapper around
   `PyGILState_Ensure`/`PyGILState_Release`) acquires the GIL for each
   `compute_sample` call and all `PyObject*` manipulation in
   `process_sample`.
3. **Shutdown**: `python_indicator_registry::reacquire_gil()` restores
   the saved thread state. `Py_Finalize` is intentionally NOT called
   (the interpreter persists for the process lifetime).

## Writing a Python indicator

### Minimal example

```python
from collections import deque

class MyMovingAverage:
    # --- Class metadata (read by C++ at discovery time) ---
    name = "My SMA"
    description = "Simple moving average"
    num_inputs = 1
    num_outputs = 1
    kind = "graph"          # "graph" | "strategy" | "orderbook" (default: "graph")
    category = "averages"

    # --- Parameter specifications for the GUI ---
    # List of (attr_name, gui_label, type_name) tuples.
    # type_name: "int", "double"/"float", "string"/"str",
    #            "ohlc_modes"/"mode", "bool"
    param_specs = [
        ("period", "Window Size", "int"),
        ("price_type", "mode", "ohlc_modes"),
    ]

    def __init__(self):
        self.period = 20
        self.price_type = "close"
        self.prices = deque(maxlen=self.period)

    def init_params(self):
        """Called by Grox to (re)set parameter defaults."""
        self.period = 20
        self.price_type = "close"

    def compute_sample(self, ohlcv: dict) -> float:
        """Compute one value per OHLCV candle.

        Args:
            ohlcv: dict with keys 'open', 'high', 'low', 'close',
                   'volume' (all float) and 'time' (int).

        Returns:
            float (single output) or list[float] (multi-output).
            int and bool returns are also accepted and coerced to float.
        """
        price_map = {
            "open":  ohlcv["open"],
            "high":  ohlcv["high"],
            "low":   ohlcv["low"],
            "close": ohlcv["close"],
            "mid_open_close": 0.5 * (ohlcv["open"] + ohlcv["close"]),
            "mid_high_low":   0.5 * (ohlcv["high"] + ohlcv["low"]),
            "volume": ohlcv["volume"],
        }
        price = price_map.get(self.price_type, ohlcv["close"])
        self.prices.append(price)
        return sum(self.prices) / len(self.prices)

    def reset(self):
        """Reset state (optional, called between runs)."""
        self.prices.clear()
```

### OHLCV dictionary

Each call to `compute_sample` receives a fresh `dict`:

| Key      | Type  | Description                 |
|----------|-------|-----------------------------|
| `open`   | float | Opening price of the candle |
| `high`   | float | Highest price in the candle |
| `low`    | float | Lowest price in the candle  |
| `close`  | float | Closing price of the candle |
| `volume` | float | Trading volume              |
| `time`   | int   | Timestamp (from the dataset) |

### Return types

`compute_sample` may return:

| Return type     | C++ `sample_result` arm         | Use case                          |
|-----------------|----------------------------------|-----------------------------------|
| `float`         | `double`                         | Single-output indicators (default) |
| `int`           | `double` (coerced)               | Integer-valued signals             |
| `bool`          | `double` (coerced to 0.0/1.0)    | Boolean signals                    |
| `list[float]`   | `std::span<float const>`         | Multi-output indicators            |
| `tuple[float]`  | `std::span<float const>`         | Multi-output indicators            |

For multi-output, set `num_outputs` to match the list length. The C++
side pre-allocates a buffer of size `num_outputs` at `init_params()`
time — no per-sample allocation.

### Class metadata

| Attribute      | Required | Default     | Description                                  |
|----------------|----------|-------------|----------------------------------------------|
| `name`         | no       | class name  | Display name in the GUI                      |
| `description`  | no       | generic     | Help text                                    |
| `num_inputs`   | no       | `1`         | Input datasets (currently only 1 supported)  |
| `num_outputs`  | no       | `1`         | Number of output series                      |
| `kind`         | no       | `"graph"`   | Indicator kind: `graph`, `strategy`, `orderbook` |
| `category`     | no       | —           | Free-form category label (not read by C++)   |
| `param_specs`  | no       | —           | List of `(attr_name, gui_label, type_name)` tuples |

### Discovery

Python indicators are discovered by scanning `.py` files in the plugin
directory for classes with a `compute_sample` method (classes starting
with `_` are skipped). No explicit export list is needed — the
`GROX_PYTHON_INDICATORS` list is **not read** by C++ and can be
omitted.

## Building with Python support

### Prerequisites

Python 3 development headers are required. The build system uses
`find_package(Python3 COMPONENTS Interpreter Development)`.

### CMake configuration

```bash
cd /home/biddisco/build/grox
cmake -DGROX_WITH_PYTHON_INDICATORS=ON /home/biddisco/src/grox
ninja
```

If Python is not found, `GROX_WITH_PYTHON_INDICATORS` is forced `OFF`
and the build proceeds without Python support (zero runtime overhead).

### Python file installation

The CMake configuration copies all `.py` files from
`src/indicators/python/` to the build tree at
`lib/grox/plugins/python/`. An `install()` rule places them in
`CMAKE_INSTALL_LIBDIR/grox/plugins/python/` for production installs.

At runtime, Grox searches these directories for Python indicators:

- `${GROX_BINARY_DIR}/lib/grox/plugins/python`
- `../lib/grox/plugins/python` (relative to the executable)
- `./plugins/python`
- `/usr/local/lib/grox/plugins/python`

## Runtime lifecycle

```
Grox starts
│
├── python_indicator_registry::initialize()
│   └── Py_InitializeFromConfig (with auto-detected PYTHONHOME)
│
├── For each plugin directory:
│   └── load_indicators_from_directory(dir + "/python")
│       └── For each .py file (not starting with _):
│           ├── Append dir to sys.path
│           ├── PyImport_Import(module)
│           └── Scan module dict for classes with compute_sample
│               └── Store in registered_classes_ (Py_INCREF)
│
├── register_with_main_registry(registry)
│   └── For each registered class:
│       ├── Create python_indicator_wrapper(class_name, registry)
│       │   └── init_params() — read param_specs, kind, num_outputs
│       └── registry.register_indicator(wrapper)
│           └── Dispatches by wrapper->kind() → correct partition
│
├── release_gil()
│   └── PyEval_SaveThread() — pika workers can now acquire GIL
│
└── User adds Python indicator via GUI
    └── indicator_ptr(widget->get_algorithm(), hdf5_ohlc_)
        └── wrapper->create(alg, view)
            └── initialize() — create_instance(class_name)
            └── execute_from(N) — execute_streaming(N)
                └── For each sample:
                    └── process_sample(market_sample)
                        └── PyGILGuard
                        └── compute_sample_raw(ohlcv)
                        └── Coerce result → double or span<float>
```

## Performance

| Metric                    | C++       | Python          |
|---------------------------|-----------|-----------------|
| Execution per sample      | < 1 μs    | 10–50 μs        |
| Development iteration     | Hours/days| Minutes/hours   |
| Library ecosystem         | Limited   | Excellent       |

### Optimization tips

1. **Use NumPy** for array operations — much faster than Python loops
2. **Minimize allocations** — reuse buffers via `deque(maxlen=...)`
3. **Pre-compute constants** in `init_params()`
4. **Avoid heavy imports** in `compute_sample` — import at module level

## Examples

See `src/indicators/python/`:

- **`indicator_example.py`** — three working indicators:
  - `SimplePythonMovingAverage` — SMA with configurable price type
  - `ExponentialPythonMovingAverage` — EMA with exponential smoothing
  - `WeightedPythonMovingAverage` — WMA with linear weights
- **`quick_start.py`** — progressive templates:
  - `MinimalPythonIndicator` — smallest possible indicator
  - `ParameterizedIndicator` — rate of change with parameters
  - `AdvancedIndicator` — trend detector with state and signals

Each example includes a `__main__` block for local testing:

```bash
python3 src/indicators/python/quick_start.py
```

## Testing

```bash
cd /home/biddisco/build/grox
ctest -R python_indicators        # unit tests
ctest -R python_indicator_form    # GUI form test
```

The unit tests (`src/indicators/test/python_indicators.cpp`) cover:
- Module loading and class discovery
- Instance creation and lifecycle
- `compute_sample` correctness (SMA, EMA, constant data)
- Multi-instance independence
- High-frequency call stability (1000 calls)
- Parameter extraction and modification
- End-to-end registry integration (kind partitioning, `find_by_name`)

## Troubleshooting

| Problem                     | Solution                                              |
|-----------------------------|-------------------------------------------------------|
| Module not found            | Check `lib/grox/plugins/python/` exists and has `.py` files |
| Import error in Python      | Ensure all required packages are installed in the Python environment used by Grox |
| Indicator not in GUI        | Check `compute_sample` method exists on the class     |
| Crash on pika thread        | Ensure `compute_sample` returns `float`, `int`, `bool`, or `list[float]` — not `None` or other types |
| `Py_Initialize` error       | Check `PYTHONHOME` env var or `GROX_PYTHON_BASE_PREFIX` CMake define points to a valid Python installation |
| Slow computation            | Profile with `cProfile`; use NumPy instead of Python loops |

## Remaining work

- [ ] Documentation: update `PLUGIN_SYSTEM_GUIDE.md` with a Python
      indicators section
- [ ] Python indicators consuming `order_book_snapshot` (currently
      only `ohlctv_sample` is supported)
- [ ] Hot-reload of Python indicator modules at runtime
- [ ] pybind11-style C++ base class for Python indicators to inherit
      from (currently duck-typed)
- [ ] IPython integration / interactive Python console
- [ ] Performance monitoring / profiling of Python indicator execution
