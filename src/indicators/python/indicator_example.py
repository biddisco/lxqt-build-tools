"""
Example Python-based Simple Moving Average Indicator

This example demonstrates how to create a Python indicator that can be
dynamically loaded and registered in the grox system.

Python indicators inherit from the indicator framework and implement
compute_sample() to calculate values for each OHLCV candle.
"""

from collections import deque
from typing import Deque


class SimplePythonMovingAverage:
    """
    Python implementation of a Simple Moving Average

    This indicator computes a simple moving average over a configurable period.
    It demonstrates:
    - Parameter initialization via init_params()
    - State management using instance variables
    - Processing individual samples via compute_sample()
    """

    # Metadata exposed to grox system
    name = "Python SMA"
    description = "Simple Moving Average implemented in Python"
    num_inputs = 1
    num_outputs = 1
    category = "averages"

    # Parameter specifications for GUI display
    # Format: [(attribute_name, gui_label, type_name), ...]
    param_specs = [
        ("period", "Window Size", "int"),
        ("price_type", "mode", "ohlc_modes"),
    ]

    def __init__(self):
        """Initialize the indicator with default parameters."""
        self.period = 20
        self.price_type = "close"  # 'open', 'high', 'low', 'close'
        self.prices: Deque[float] = deque(maxlen=self.period)

    def init_params(self):
        """
        Called by grox to initialize parameters.
        Define all parameters needed for the indicator here.
        """
        # Period in candles
        self.period = 20

        # Which price to use: open, high, low, or close
        self.price_type = "close"

    def compute_sample(self, ohlcv_data: dict) -> float:
        """
        Compute the indicator value for a single OHLCV sample.

        Args:
            ohlcv_data: Dictionary with keys: 'open', 'high', 'low', 'close', 'volume', 'time'

        Returns:
            The computed indicator value (SMA for this example)
        """
        # Extract the price based on configured type
        price_map = {
            "open": ohlcv_data["open"],
            "high": ohlcv_data["high"],
            "low": ohlcv_data["low"],
            "close": ohlcv_data["close"],
            "mid_open_close": 0.5 * (ohlcv_data["open"] + ohlcv_data["close"]),
            "mid_high_low": 0.5 * (ohlcv_data["high"] + ohlcv_data["low"]),
            "volume": ohlcv_data["volume"],
            "value": ohlcv_data["volume"]
            * (0.5 * (ohlcv_data["open"] + ohlcv_data["close"])),
        }

        price = price_map.get(self.price_type, ohlcv_data["close"])

        # Add to buffer
        self.prices.append(price)

        # Compute simple moving average
        if len(self.prices) == 0:
            return 0.0

        return sum(self.prices) / len(self.prices)

    def set_period(self, period: int):
        """Dynamically change the averaging period."""
        self.period = max(1, period)
        self.prices = deque(maxlen=self.period)

    def get_period(self) -> int:
        """Get current averaging period."""
        return self.period

    def reset(self):
        """Reset the indicator state."""
        self.prices.clear()


class ExponentialPythonMovingAverage:
    """
    Python implementation of an Exponential Moving Average

    Demonstrates more complex state management and parameter handling.
    """

    name = "Python EMA"
    description = "Exponential Moving Average implemented in Python"
    num_inputs = 1
    num_outputs = 1
    category = "averages"

    # Parameter specifications for GUI display
    param_specs = [
        ("period", "Window Size", "int"),
    ]

    def __init__(self):
        """Initialize the exponential moving average."""
        self.period = 20
        self.alpha = 2.0 / (self.period + 1)
        self.ema_value = None
        self.sample_count = 0

    def init_params(self):
        """Initialize parameters."""
        self.period = 20
        self.alpha = 2.0 / (self.period + 1)
        self.ema_value = None
        self.sample_count = 0

    def compute_sample(self, ohlcv_data: dict) -> float:
        """
        Compute exponential moving average for a sample.

        The EMA gives more weight to recent prices through exponential smoothing.
        """
        close = ohlcv_data["close"]

        if self.ema_value is None:
            # First sample - initialize with close price
            self.ema_value = close
        else:
            # EMA = (Close - Previous_EMA) * Alpha + Previous_EMA
            self.ema_value = (close - self.ema_value) * self.alpha + self.ema_value

        self.sample_count += 1
        return self.ema_value

    def set_period(self, period: int):
        """Change the EMA period."""
        self.period = max(1, period)
        self.alpha = 2.0 / (self.period + 1)

    def reset(self):
        """Reset to uninitialized state."""
        self.ema_value = None
        self.sample_count = 0


class WeightedPythonMovingAverage:
    """
    Python implementation of a Weighted Moving Average

    Each sample is weighted linearly:
    WMA = (price1*w1 + price2*w2 + ... + priceN*wN) / (w1 + w2 + ... + wN)
    where weights increase linearly: 1, 2, 3, ..., N
    """

    name = "Python WMA"
    description = "Weighted Moving Average implemented in Python"
    num_inputs = 1
    num_outputs = 1
    category = "averages"

    # Parameter specifications for GUI display
    param_specs = [
        ("period", "Window Size", "int"),
    ]

    def __init__(self):
        self.period = 20
        self.prices: Deque[float] = deque(maxlen=self.period)

    def init_params(self):
        self.period = 20
        self.prices = deque(maxlen=self.period)

    def compute_sample(self, ohlcv_data: dict) -> float:
        """Compute weighted moving average."""
        close = ohlcv_data["close"]
        self.prices.append(close)

        if len(self.prices) == 0:
            return 0.0

        # Calculate weights: 1, 2, 3, ..., N for oldest to newest
        weights = list(range(1, len(self.prices) + 1))
        weighted_sum = sum(p * w for p, w in zip(self.prices, weights))
        weight_sum = sum(weights)

        return weighted_sum / weight_sum if weight_sum > 0 else 0.0

    def set_period(self, period: int):
        self.period = max(1, period)
        self.prices = deque(maxlen=self.period)

    def reset(self):
        self.prices.clear()


# Export registry for grox plugin system
# This list tells grox which Python classes are available as indicators
GROX_PYTHON_INDICATORS = [
    SimplePythonMovingAverage,
    ExponentialPythonMovingAverage,
    WeightedPythonMovingAverage,
]

__all__ = [
    "SimplePythonMovingAverage",
    "ExponentialPythonMovingAverage",
    "WeightedPythonMovingAverage",
]
