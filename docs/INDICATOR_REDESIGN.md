# Indicator Redesign

## Overview

A multi-phase refactoring of the indicator system to introduce a
unified `process_sample` streaming interface and a single registry
partitioned by `indicator_kind`, eliminating the old `call_operator`
machinery and the split `available_indicators` / `available_arbitragers`
globals.

---

## Phase 0 — Scaffolding (DONE)

Added the type infrastructure with no behaviour change:

- `indicator_kind` enum (graph / strategy / orderbook)
- `market_sample` variant (ohlctv_sample | order_book_snapshot)
- `sample_result` variant (double | span<float const> | buy_sell_point)
- `output_descriptor` + `get_output_descriptors()` virtual
- `clone()` virtual on `algorithm_base`
- `portfolio` type extracted from `trade_sell_sliding_stop`

Commit: `9123e18`

---

## Phase 1 — Convert to process_sample (DONE)

Replaced the per-indicator `call_operator_ohlc_*` / `call_operator_buy_sell`
methods and `call_helper` with a single `process_sample(market_sample)`
virtual. The base class provides `execute_streaming(N)` which iterates
samples, calls `process_sample`, and fans results into named outputs via
`get_output_descriptors()`. Added `FACTORY_INDICATOR_V2` macro to
implement `clone()`, `create()`, `execute_from`, `execute_continue`.

All indicator `.hpp` files converted. Old `call_helper`, `call_operator_*`,
and `FACTORY_INDICATOR_CREATE` deleted.

Commits: `46c93fa` → `9d15189`

---

## Phase 2 — Registry Unification

### Problem

The registry is currently split into two global vectors —
`available_indicators` and `available_arbitragers` — partitioned by
*which registration function the plugin calls*, not by the indicator's
actual `kind()`. This diverges from the `indicator_kind` enum added in
Phase 0:

| Indicator               | `kind()`   | Registered via     | Lands in            |
|-------------------------|------------|--------------------|---------------------|
| trade_sell_sliding_stop | strategy   | register_indicator | available_indicators|
| trade_rebalance_funds   | strategy   | register_indicator | available_indicators|
| trade_arbitrage_2_way   | orderbook  | register_arbitrage | available_arbitragers|
| trade_currency_exchange | orderbook  | register_arbitrage | available_arbitragers|
| trade_market_maker      | orderbook  | register_arbitrage | available_arbitragers|
| (all graph indicators)  | graph      | register_indicator | available_indicators|

### Goal

Collapse the two global vectors into a single registry partitioned by
`indicator_kind`. A single `register()` method dispatches by `p->kind()`.
Consumers (GUI widgets) query the registry by kind instead of accessing
global vectors directly.

### Steps

1. **Unify registry storage** — Replace the two `extern` global vectors
   with a single partitioned container inside the `indicator_registry`
   singleton (e.g. `std::array<indicator_vector, 3>` indexed by
   `indicator_kind`). Add `by_kind(indicator_kind)` accessor returning
   `indicator_vector const*`.

2. **Single register() entry point** — Replace `register_indicator` +
   `register_arbitrage` with one `register(shared_algorithm p)` that
   dispatches via `p->kind()`. Keep `register_indicator` as a thin
   inline alias for back-compat during the transition if needed.

3. **Update plugins** — `src/indicators/plugins/trading_plugin.cpp`:
   change `register_arbitrage` calls to `register`. Other plugins
   already call `register_indicator` — no change needed.

4. **Update price_chart_widget** — Replace
   `&indicators::available_indicators` with
   `indicator_registry::getInstance().by_kind(indicator_kind::graph)`.
   Preserve the static selection index.

5. **Update trade_algorithm_widget** — Uncomment and replace
   `&indicators::available_arbitragers` with kind-filtered views
   (strategy + orderbook for the trading dialog).

6. **Remove globals, finalize find_by_name** — Remove the `extern`
   global vectors. `find_by_name` searches all partitions of the
   unified registry.

### Touchpoints

| File                                         | Change                              |
|----------------------------------------------|-------------------------------------|
| `src/indicators/indicator_registry.hpp`      | Unified storage, single register()  |
| `src/indicators/indicator_registry.cpp`      | Definitions, find_by_name           |
| `src/indicators/plugins/trading_plugin.cpp`  | register_arbitrage → register       |
| `src/widgets/price_chart_widget.cpp`         | Global → by_kind(graph)             |
| `src/widgets/trade_algorithm_widget.cpp`     | Global → by_kind(strategy/orderbook)|
| `src/widgets_control/indicator_widget.hpp`   | (possibly) accept kind-filtered vec |
| `src/widgets_control/test/indicator_form.cpp | Verify still works                  |
| `src/apps/grox/main.cpp`                     | Verify find_by_name                 |
| `src/apps/grox/mainwindow.cpp`              | Verify find_by_name                 |

### Non-goals (Phase 3)

- Plugin loader changes (already passes registry by reference)
- Python indicator registry (already calls `register_indicator`)
- `indicator_ptr` or `indicators_model` (operate on live instances, not prototypes)
- `emit-to-sink` refactor (separate future work)

---

## Phase 3 — Python Indicator Integration

### Current state

The Python integration is **substantially implemented** (1254 lines in
`python_plugin.cpp`) but has never worked end-to-end as expected. The
unit tests pass (they test `compute_sample` in isolation), but the
full GUI → registry → execute → plot pipeline has gaps:

**What works:**
- Python interpreter initialization via `PyConfig` API
- Module discovery (scans `.py` files for classes with `compute_sample`)
- Instance creation and `compute_sample` calls via raw Python C API
- GIL release/reacquire for pika worker threads
- Parameter extraction from `param_specs` class attribute
- Registration with the main C++ registry (all Python indicators land
  in `indicators_by_kind_[graph]`)

**What's broken or missing:**
1. **No kind from Python** — `python_indicator_wrapper` doesn't override
   `kind()`, so all Python indicators are `graph`. Python classes can't
   declare their kind.
2. **No C++/Python distinction** — Python and C++ graph indicators are
   indistinguishable in the unified registry. No `source` field, no
   sub-category, no way to filter in the GUI.
3. **`compute_sample` return must be `PyFloat_Check`** — integer
   returns (e.g. `0`, `1`, `-1` for signal indicators) are silently
   rejected and return `0.0` (python_plugin.cpp:559-565).
4. **`python_bindings.hpp` is dead code** — the original pybind11-style
   design (`py_algorithm_base`, `py_indicator_base`) was never wired up.
   The real integration uses raw Python C API with duck typing.
5. **`GROX_PYTHON_INDICATORS` export list is never read** — C++
   discovers classes by scanning for `compute_sample` attributes, not
   by reading the export list.
6. **Python files not installed** — no `install()` rule for
   `src/indicators/python/*.py`. Users must manually copy them to
   `lib/grox/plugins/python/`.
7. **`num_outputs() = 1` hardcoded** — Python indicators can't produce
   multiple output series (e.g. Bollinger Bands with upper/lower/mid).
8. **GIL management gaps** — `set_parameter<double>` and
   `set_parameter<string>` don't acquire the GIL (only safe because
   all current callers hold it).
9. **Documentation stale** — `PYTHON_IMPLEMENTATION_SUMMARY.md` and
   `PYTHON_INDICATORS_DELIVERY.md` still describe python_plugin.cpp
   as a "180-line stub" and reference wrong file paths.
10. **`python_indicator_registry` is a separate singleton** — could
    fold into the main `indicator_registry` to simplify the architecture.

### Design decisions

1. **Kind from Python** — Add an optional `kind` class attribute to the
   Python indicator protocol (default `"graph"`). The
   `python_indicator_wrapper` reads it at `init_params()` time and
   overrides `kind()` to return the Python-declared value. This means a
   Python indicator can be `graph`, `strategy`, or `orderbook` —
   matching the C++ convention.

2. **Source field (C++ vs Python)** — Add a `source()` virtual to
   `algorithm_base` returning an enum `{ cpp, python }`. Default is
   `cpp`; `python_indicator_wrapper` overrides to `python`. This is a
   lightweight sub-category that doesn't affect registry partitioning
   (which is by `kind()`) but lets the GUI display an icon or filter.
   The `source()` method is orthogonal to `kind()` — kind is about
   behavior, source is about implementation.

3. **Fold python registry into main** — The `python_indicator_registry`
   singleton remains as the Python interpreter manager (it owns the
   GIL, loads modules, creates instances), but its
   `register_with_main_registry` already bridges into the main
   `indicator_registry`. No structural change needed here — the bridge
   works. The only improvement is that Python indicators now land in
   the correct `kind()` partition instead of always `graph`.

4. **`get_available_indicators()` alignment** — The Python registry's
   `get_available_indicators()` method returns a list of Python class
   names. This is a Python-registry-specific API for debugging/testing,
   not a consumer-facing API. Consumers use `indicator_registry::by_kind()`
   which already includes Python wrappers. No change needed.

5. **Kind is a convenience label** — Per the user's guidance, `kind()`
   is not a deep semantic distinction. A trading indicator produces
   graphs too; a graph indicator could be extended to support trading.
   The `kind()` enum drives registry partitioning and GUI grouping, but
   should not gate execution or prevent an indicator from appearing in
   multiple contexts.

### Steps

#### Step 1: Add `source()` to `algorithm_base`
- Add `enum class indicator_source : int { cpp, python }`
- Add `virtual indicator_source source() const { return indicator_source::cpp; }`
  to `algorithm_base.hpp`
- Add `indicator_kind` and `indicator_source` to `indicator_types.hpp`
  (if not already there)

#### Step 2: Python indicator protocol — `kind` attribute
- Add optional `kind` class attribute to Python indicator protocol
  (default `"graph"`, one of `"graph"`, `"strategy"`, `"orderbook"`)
- Add optional `num_outputs` class attribute (default `1`) — already
  exists in examples but is not read by C++
- Update `indicator_example.py` to declare `kind = "graph"` on all
  three example classes

#### Step 3: `python_indicator_wrapper` — override `kind()` and `source()`
- In `init_params()`, read the Python class's `kind` attribute (default
  `"graph"`) and store it as a member
- Override `kind()` to return the stored value
- Override `source()` to return `indicator_source::python`
- Read `num_outputs` from the Python class (default `1`) and override
  `num_outputs()` to return it

#### Step 4: Fix `compute_sample` return type handling
- Accept `int` returns by converting to `float` (PyLong_AsDouble)
- Accept `bool` returns by converting to `float` (0.0 or 1.0)
- Keep `float` as the primary return type
- Log a warning (not error) for unexpected types, then attempt
  conversion before giving up

#### Step 5: Multi-output Python indicators
- When `num_outputs > 1`, the Python `compute_sample` can return either:
  - A single `float` (written to output 0 only)
  - A `list` or `tuple` of floats (written to outputs 0..N-1)
- Update `python_indicator_wrapper::process_sample` to handle the list
  case by returning `std::span<float const>` into a pre-allocated buffer
  (already supported by `sample_result` variant and `execute_streaming`)
- Size the `output_buffer_` member to `num_outputs()` at `initialize()`
  time (base class already does this)

#### Step 6: Remove dead code
- Delete `src/indicators/python_bindings.hpp` (unused pybind11-style
  bindings)
- Remove the `GROX_PYTHON_INDICATORS` export list from Python example
  files (never read by C++)
- Remove `py_main_module_` dead member from `python_indicator_registry`

#### Step 7: GIL safety audit
- Add `PyGILGuard` to `set_parameter<double>` and `set_parameter<string>`
- Add null check for `PySys_GetObject("path")` in `load_module`
- Add null check for `dynamic_cast` result in `python_indicator_wrapper::create`
- Fix `std::wstring` construction from `const char*` to use
  `Py_DecodeLocale` for non-ASCII PYTHONHOME paths

#### Step 8: Install Python indicator files
- Add CMake `install()` rule for `src/indicators/python/*.py` to
  `lib/grox/plugins/python/`
- Ensure the build tree also has a copy for development
  (e.g. a custom target that copies to `${GROX_BINARY_DIR}/lib/grox/plugins/python/`)

#### Step 9: Documentation
- Rewrote `docs/PYTHON_INDICATORS.md` as a single master doc covering
  architecture, the Python indicator protocol, build configuration,
  runtime lifecycle, examples, testing, and troubleshooting
- Deleted `docs/PYTHON_IMPLEMENTATION_SUMMARY.md`,
  `docs/PYTHON_INDICATORS_DELIVERY.md`, and `docs/ARCHITECTURE.md`
  (all stale — described python_plugin.cpp as a "180-line stub",
  referenced deleted files like python_bindings.hpp)

#### Step 10: End-to-end test
- Add a test that loads `indicator_example.py`, registers with the main
  registry, and verifies the Python SMA appears in
  `by_kind(indicator_kind::graph)` alongside C++ indicators
- Add a test that creates a `python_indicator_wrapper` with
  `kind = "strategy"` and verifies it lands in
  `by_kind(indicator_kind::strategy)`
- Verify the existing GUI test (`python_indicator_form.cpp`) still works

### Touchpoints

| File                                          | Change                                    |
|-----------------------------------------------|-------------------------------------------|
| `src/indicators/indicator_types.hpp`         | Add `indicator_source` enum               |
| `src/indicators/algorithm_base.hpp`          | Add `source()` virtual                    |
| `src/indicators/python_plugin.hpp`           | Add `kind_`, `source()` override, remove dead member |
| `src/indicators/python_plugin.cpp`           | Read `kind`/`num_outputs` from Python, fix return type, fix GIL, fix multi-output |
| `src/indicators/python_bindings.hpp`         | Delete (dead code)                        |
| `src/indicators/python/indicator_example.py` | Add `kind = "graph"` declarations         |
| `src/indicators/python/quick_start.py`       | Add `kind = "graph"` declarations         |
| `src/indicators/CMakeLists.txt`             | Remove `python_bindings.hpp`, add install |
| `src/indicators/test/python_indicators.cpp`  | Add end-to-end registry tests             |
| `docs/PYTHON_INDICATORS.md`                  | Rewritten as master doc                  |
| `docs/PYTHON_IMPLEMENTATION_SUMMARY.md`      | Deleted (stale)                           |
| `docs/PYTHON_INDICATORS_DELIVERY.md`         | Deleted (stale)                           |
| `docs/ARCHITECTURE.md`                       | Deleted (stale, folded into master doc)   |
| `docs/PLUGIN_SYSTEM_GUIDE.md`               | Add Python indicators section (todo)      |

### Non-goals (future work)

- Hot-reload of Python indicator modules at runtime
- pybind11-style C++ base class for Python indicators to inherit from
- IPython integration / interactive Python console
- Python indicators consuming `order_book_snapshot` (currently only
  `ohlctv_sample` is supported)
- Performance monitoring / profiling of Python indicator execution

