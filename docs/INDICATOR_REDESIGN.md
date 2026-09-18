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

## Phase 3 — Plugin + Python Update

Adapt the plugin system and Python indicator integration to the unified
registry. Details to be planned after Phase 2 lands.
