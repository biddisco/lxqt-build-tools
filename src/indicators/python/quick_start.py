#!/usr/bin/env python3
"""
Quick Start Guide for Python Indicators in Grox

This script shows the minimal setup needed to create a working Python indicator.
Copy this as a template for your own indicators.
"""

# ==============================================================================
# MINIMAL EXAMPLE: Simple Moving Average
# ==============================================================================

from collections import deque


class MinimalPythonIndicator:
    """
    Minimal working indicator example.
    Just copy this class and modify to create your own!
    """

    # These attributes are required by Grox
    name = "My First Indicator"
    description = "A minimal indicator example"
    num_inputs = 1
    num_outputs = 1
    category = "custom"
    # Optional: declare indicator kind (graph, strategy, orderbook).
    # Defaults to "graph" if omitted.
    kind = "graph"

    def __init__(self):
        """Initialize your indicator state here."""
        self.period = 10
        self.prices = deque(maxlen=self.period)

    def init_params(self):
        """
        Called by Grox to initialize parameters.
        This is where you set defaults for any tunable values.
        """
        self.period = 10

    def compute_sample(self, ohlcv_data: dict) -> float:
        """
        Calculate indicator value for one OHLCV candle.

        This is called once per candle in the dataset.

        IMPORTANT: Must return a single float!

        Args:
            ohlcv_data: Dictionary with keys:
                - 'open': float
                - 'high': float
                - 'low': float
                - 'close': float
                - 'volume': float
                - 'time': int

        Returns:
            float: A single computed value
        """
        # Get the closing price
        close = ohlcv_data["close"]

        # Add to buffer
        self.prices.append(close)

        # Calculate simple moving average
        if len(self.prices) > 0:
            return sum(self.prices) / len(self.prices)
        else:
            return close

    # Optional: Override these methods as needed
    def reset(self):
        """Reset state between different runs"""
        self.prices.clear()


# ==============================================================================
# INTERMEDIATE EXAMPLE: Indicator with Parameters
# ==============================================================================


class ParameterizedIndicator:
    """
    Example showing how to use parameters that can be set at runtime.
    """

    name = "Rate of Change"
    description = "Calculates price rate of change"
    num_inputs = 1
    num_outputs = 1
    category = "momentum"
    kind = "graph"

    def __init__(self):
        self.period = 12  # Number of periods to look back
        self.price_type = "close"  # Which price to use
        self.history = deque(maxlen=self.period + 1)

    def init_params(self):
        """Initialize parameter defaults"""
        self.period = 12
        self.price_type = "close"

    def compute_sample(self, ohlcv_data: dict) -> float:
        """
        Rate of Change = (Current Price - Price N periods ago) / Price N periods ago
        """
        # Extract price based on configuration
        price_map = {
            "open": ohlcv_data["open"],
            "high": ohlcv_data["high"],
            "low": ohlcv_data["low"],
            "close": ohlcv_data["close"],
        }
        current_price = price_map.get(self.price_type, ohlcv_data["close"])

        self.history.append(current_price)

        # Need minimum history to calculate ROC
        if len(self.history) < 2:
            return 0.0

        # Most recent price
        recent = self.history[-1]

        # Price from N periods ago (or oldest in buffer)
        previous = self.history[0]

        # Calculate percentage change
        if previous == 0:
            return 0.0

        roc = (recent - previous) / abs(previous) * 100
        return roc

    def set_period(self, period: int):
        """Allow changing period at runtime"""
        self.period = max(1, period)
        # Resize history buffer
        old_history = list(self.history)
        self.history = deque(maxlen=self.period + 1)
        self.history.extend(old_history[-self.period - 1 :])

    def reset(self):
        """Reset for new dataset"""
        self.history.clear()


# ==============================================================================
# ADVANCED EXAMPLE: Stateful Indicator with Complex Logic
# ==============================================================================


class AdvancedIndicator:
    """
    More complex example showing state management and signal generation.
    """

    name = "Trend Detector"
    description = "Detects trend changes using moving averages"
    num_inputs = 1
    num_outputs = 1
    category = "advanced"
    kind = "graph"

    def __init__(self):
        self.fast_period = 10
        self.slow_period = 20
        self.fast_prices = deque(maxlen=self.fast_period)
        self.slow_prices = deque(maxlen=self.slow_period)
        self.last_signal = 0
        self.crossover_count = 0

    def init_params(self):
        """Set up parameters"""
        self.fast_period = 10
        self.slow_period = 20

    def compute_sample(self, ohlcv_data: dict) -> float:
        """
        Generate signal based on moving average crossover:
        - +1.0 when fast MA crosses above slow MA (bullish)
        - -1.0 when fast MA crosses below slow MA (bearish)
        -  0.0 otherwise
        """
        close = ohlcv_data["close"]

        # Add to buffers
        self.fast_prices.append(close)
        self.slow_prices.append(close)

        # Calculate moving averages
        fast_ma = (
            sum(self.fast_prices) / len(self.fast_prices) if self.fast_prices else close
        )
        slow_ma = (
            sum(self.slow_prices) / len(self.slow_prices) if self.slow_prices else close
        )

        # Signal remains 0 until we have enough history
        if (
            len(self.fast_prices) < self.fast_period
            or len(self.slow_prices) < self.slow_period
        ):
            return 0.0

        # Determine signal
        signal = 0.0
        if fast_ma > slow_ma:
            signal = 1.0  # Uptrend
        elif fast_ma < slow_ma:
            signal = -1.0  # Downtrend

        # Detect crossovers
        if signal != self.last_signal and self.last_signal != 0:
            self.crossover_count += 1

        self.last_signal = signal
        return signal

    def reset(self):
        """Reset state"""
        self.fast_prices.clear()
        self.slow_prices.clear()
        self.last_signal = 0
        self.crossover_count = 0


# ==============================================================================
# EXPORT: Tell Grox about your indicators
# ==============================================================================

# This list tells Grox which classes in this file are indicators
GROX_PYTHON_INDICATORS = [
    MinimalPythonIndicator,
    ParameterizedIndicator,
    AdvancedIndicator,
]


# ==============================================================================
# HOW TO USE THIS FILE
# ==============================================================================

if __name__ == "__main__":
    """
    Test your indicators locally before deploying to Grox.
    """
    print("Testing Python Indicators Locally\n" + "=" * 50)

    # Test data: Simulated OHLCV candles
    test_candles = [
        {"open": 100, "high": 102, "low": 99, "close": 101, "volume": 1000, "time": i}
        for i in range(100)
    ]

    # Add some trend
    for i in range(50):
        test_candles[i + 50]["close"] += i * 0.5

    # Create and test indicator
    indicator = MinimalPythonIndicator()
    indicator.init_params()

    print(f"\nTesting: {indicator.name}")
    print(f"Description: {indicator.description}")

    results = []
    for candle in test_candles:
        result = indicator.compute_sample(candle)
        results.append(result)

    print(f"Processed {len(results)} candles")
    print(f"First 5 values:  {results[:5]}")
    print(f"Last 5 values:   {results[-5:]}")
    print(f"Min value:  {min(results):.2f}")
    print(f"Max value:  {max(results):.2f}")
    print(f"Mean value: {sum(results) / len(results):.2f}")

    print("\n✓ Indicator works locally!")
    print("  Now copy this file to: ${GROX_BUILD}/lib/grox/plugins/python/")
    print("  Then run: ./bin/grox")
