# Indicators Plugin System

## Overview

All indicators — C++ and Python — are loaded as plugins at runtime.
The C++ plugin system uses `dlopen`/`dlsym` to load shared libraries
from a plugin directory; Python indicators are discovered separately by
the Python registry (see [PYTHON_INDICATORS.md](PYTHON_INDICATORS.md)).
Both paths register into a single unified `indicator_registry`
partitioned by `kind()`.

```
Grox Application (bin/grox)
│
├── indicator_registry::getInstance()  (singleton)
│   └── indicators_by_kind_[graph | strategy | orderbook]
│       ├── C++ indicators    (source() == cpp)
│       └── Python wrappers   (source() == python)
│
├── plugin_loader: dlopen .so files → register_indicator()
│   ├── moving_averages.so
│   ├── volatility.so
│   ├── technical_indicators.so
│   └── trading.so
│
└── python_indicator_registry: scan .py files → register_with_main_registry()
    └── lib/grox/plugins/python/*.py
```

## Key components

### 1. Plugin API (`src/indicators/plugin_api.hpp`)

Defines the C-compatible ABI for `.so` plugins:

- `grox_plugin_get_info()` — returns `grox_plugin_info` (name, version,
  description, category, api_version)
- `grox_plugin_register(indicator_registry* registry)` — entry point
  called by the loader; the plugin calls `registry->register_indicator()`
  for each indicator it provides
- Convenience macros: `GROX_DEFINE_PLUGIN_INFO`,
  `GROX_BEGIN_PLUGIN_REGISTRATION`, `GROX_END_PLUGIN_REGISTRATION`
- API version: `1.0` (major.minor encoded as uint32). The loader
  rejects plugins with an incompatible major version.

### 2. Plugin loader (`src/indicators/plugin_loader.{hpp,cpp}`)

- Scans directories for `.so` files
- Uses `dlopen()` / `dlsym()` to load plugins dynamically
- Validates API version compatibility
- Calls each plugin's `grox_plugin_register()` function, passing the
  registry by reference

### 3. Indicator registry (`src/indicators/indicator_registry.{hpp,cpp}`)

- Singleton managing all registered indicators
- Partitioned by `indicator_kind` (graph, strategy, orderbook) via
  `p->kind()`
- `register_indicator(shared_algorithm p)` dispatches by kind
- `by_kind(indicator_kind)` returns a filtered view for consumers
- `find_by_name(name)` searches all partitions
- `load_plugins_from_directory(dir)` loads all `.so` files from a
  directory

## Build system

### Core library (`grox_indicators`)

Contains base classes (`algorithm_base`, `indicator_base`), plugin
infrastructure (`plugin_api.hpp`, `plugin_loader`), and the registry.
Links to `${CMAKE_DL_LIBS}` for `dlopen()` support.

### Plugin macro (`add_indicator_plugin`)

Defined in `src/indicators/CMakeLists.txt`:

```cmake
add_indicator_plugin(
  NAME     grox_plugin_volatility
  SOURCES  plugins/volatility_plugin.cpp
  HEADERS  volatility_bollinger_bands.hpp
           volatility_garman_klass.hpp
           volatility_rogers_satchell.hpp
  OUTPUT_NAME volatility
)
```

The macro handles:
- Shared library creation with proper visibility
- Output to `lib/grox/plugins/` in the build tree
- Versioned symlinks (`.so`, `.so.1`, `.so.1.0.0`)
- Install to `CMAKE_INSTALL_LIBDIR/grox/plugins/`

### Current plugins

| Plugin           | File                          | Indicators                                     |
|------------------|-------------------------------|------------------------------------------------|
| moving_averages  | `plugins/moving_averages_plugin.cpp` | SMA, EMA, EMVW, HMA, VWMA, MACD, Cross   |
| volatility       | `plugins/volatility_plugin.cpp`     | Bollinger, Garman-Klass, Rogers-Satchell |
| technical_indicators | `plugins/technical_indicators_plugin.cpp` | RSI, Stoch, StochRSI, ATR, CCI, ROC, WPR, OBV, ADX, PSAR, Ichimoku, VWAP |
| trading          | `plugins/trading_plugin.cpp`        | Arbitrage 2-way, Currency-Exchange, Market-Maker, Sliding Stop, Rebalance Funds |

## Runtime loading

### Initialization sequence (`src/apps/grox/main.cpp`)

```cpp
auto& registry = indicators::indicator_registry::getInstance();

// 1. Load C++ plugins (.so files)
for (auto const& dir : plugin_dirs)
    registry.load_plugins_from_directory(dir);

// 2. Load Python indicators (.py files) — if enabled
auto& py_registry = indicators::python::python_indicator_registry::instance();
py_registry.initialize();
for (auto const& dir : plugin_dirs)
    py_registry.load_indicators_from_directory(dir + "/python");
py_registry.register_with_main_registry(registry);
py_registry.release_gil();
```

### Plugin search directories

The system tries multiple locations:

1. `${GROX_BINARY_DIR}/lib/grox/plugins`
2. `../lib/grox/plugins` (relative to the executable)
3. `./plugins`
4. `/usr/local/lib/grox/plugins`

Python indicators are searched in `<each dir>/python`.

## Creating a new C++ plugin

### Step 1: Write the plugin file

Create `src/indicators/plugins/my_custom_plugin.cpp`:

```cpp
#include "indicators/plugin_api.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/my_custom_indicator.hpp"

GROX_DEFINE_PLUGIN_INFO("My Custom Indicators", "1.0.0",
    "Description of what this plugin provides", "custom")

GROX_BEGIN_PLUGIN_REGISTRATION()
{
    auto ind = std::make_shared<indicators::my_custom_indicator>();
    ind->init_params();
    registry->register_indicator(ind);
}
GROX_END_PLUGIN_REGISTRATION()
```

### Step 2: Add to CMakeLists.txt

In `src/indicators/CMakeLists.txt`:

```cmake
add_indicator_plugin(
  NAME     grox_plugin_custom
  SOURCES  plugins/my_custom_plugin.cpp
  HEADERS  my_custom_indicator.hpp
  OUTPUT_NAME custom
)
```

### Step 3: Build

```bash
cd /home/biddisco/build/grox
ninja grox_plugin_custom
```

Result: `lib/grox/plugins/custom.so`

## Creating a Python indicator

See [PYTHON_INDICATORS.md](PYTHON_INDICATORS.md) for the full guide.
Briefly:

```python
class MyIndicator:
    name = "My Indicator"
    kind = "graph"          # graph | strategy | orderbook
    num_outputs = 1
    param_specs = [("period", "Window Size", "int")]

    def __init__(self):
        self.period = 20

    def compute_sample(self, ohlcv: dict) -> float:
        return ohlcv["close"]
```

Place `.py` files in `lib/grox/plugins/python/`. They are discovered
automatically by scanning for classes with a `compute_sample` method.

## GUI generation

GUI generation is fully decoupled. When a plugin registers an
indicator:

1. The indicator's `init_params()` method populates `param_list`
2. The `control_factory` builds Qt widgets from param metadata
3. No plugin-specific GUI code needed

This means:
- Plugins need only implement `init_params()`
- GUI is automatically generated from param types
- User selections are serializable as JSON

## Indicator classification

Every indicator has two orthogonal classifiers:

| Classifier  | Method          | Values                          | Purpose                         |
|-------------|-----------------|---------------------------------|---------------------------------|
| `kind()`    | virtual         | `graph`, `strategy`, `orderbook`| Registry partitioning, GUI grouping |
| `source()`  | virtual         | `cpp`, `python`                 | Implementation origin, GUI icon  |

`kind()` drives which partition the indicator lands in and how the GUI
groups it. `source()` is a display hint — C++ and Python indicators
with the same `kind()` are interchangeable. Kind is a convenience
label: a trading indicator produces graphs too, and a graph indicator
could be extended to support trading.

## Debugging

### Check plugin loading at startup

```bash
./bin/grox 2>&1 | grep -i plugin
```

### Inspect plugin symbols

```bash
nm -D lib/grox/plugins/moving_averages.so | grep grox_plugin
```

### Test plugin loading

```cpp
auto& registry = indicators::indicator_registry::getInstance();
auto loaded = registry.load_plugins_from_directory("./test/plugins");
std::cout << "Loaded " << loaded << " plugins\n";
```

## External projects

Plugins can be developed in separate projects:

```
my_custom_indicators/
├── CMakeLists.txt
├── src/
│   └── custom_plugin.cpp
└── README.md
```

```cmake
cmake_minimum_required(VERSION 3.20)
project(grox_custom_indicators)

find_package(grox REQUIRED)

add_library(grox_plugin_custom SHARED src/custom_plugin.cpp)
target_link_libraries(grox_plugin_custom PRIVATE grox::indicators)

install(TARGETS grox_plugin_custom
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/grox/plugins
)
```

## References

- Plugin API: [src/indicators/plugin_api.hpp](../src/indicators/plugin_api.hpp)
- Plugin loader: [src/indicators/plugin_loader.hpp](../src/indicators/plugin_loader.hpp)
- Registry: [src/indicators/indicator_registry.hpp](../src/indicators/indicator_registry.hpp)
- Example plugin: [src/indicators/plugins/moving_averages_plugin.cpp](../src/indicators/plugins/moving_averages_plugin.cpp)
- Python indicators: [PYTHON_INDICATORS.md](PYTHON_INDICATORS.md)
- Indicator redesign plan: [INDICATOR_REDESIGN.md](INDICATOR_REDESIGN.md)
