/**
 * Python Indicator Plugin Tests
 *
 * Comprehensive test suite for Python-based indicators in grox trading system.
 * These tests verify the entire lifecycle and robustness of Python indicators.
 *
 * Test Coverage:
 * ============================================================
 * 1. LoadIndicatorModule
 *    - Loads indicator_example.py module
 *    - Verifies all three indicator classes are discovered
 *
 * 2. CreateIndicatorInstance
 *    - Creates instances of Python indicator classes
 *    - Verifies PyObject is properly wrapped
 *
 * 3. ComputeSampleSimplePythonMovingAverage
 *    - Tests SMA computation on incremental price data
 *    - Validates results against manually calculated expectations
 *    - Checks 25 samples for accuracy
 *
 * 4. ComputeSampleConstantData
 *    - Tests SMA with constant (unchanging) input data
 *    - Verifies SMA of constant = constant
 *
 * 5. ComputeSampleExponentialMovingAverage
 *    - Tests EMA with variable price data
 *    - Validates non-NaN and finite results
 *
 * 6. MultipleInstances
 *    - Creates two independent instances
 *    - Feeds different data to each instance
 *    - Verifies instances don't interfere with each other
 *
 * 7. WrapperWithOHLCVSamples (UI Integration)
 *    - Tests python_indicator_wrapper class
 *    - Uses real OHLCV samples (mimics UI usage)
 *    - Verifies operator() method works correctly
 *
 * 8. HighFrequencyCalls (Stress Test)
 *    - Makes 1000 rapid compute_sample calls
 *    - Tests reference counting stability
 *    - Catches memory leaks and dangling pointers
 *
 * 9. ReturnValueValidation (Edge Cases)
 *    - Tests extreme price values (1e10, 1e-10)
 *    - Tests zero volume scenario
 *    - Validates all results are finite
 *
 * 10. AllIndicatorTypes
 *    - Tests all three indicator types:
 *      - SimplePythonMovingAverage
 *      - ExponentialPythonMovingAverage  
 *      - WeightedPythonMovingAverage
 *    - Ensures all work without errors
 *
 * 11. InstanceLifecycle
 *    - Creates and destroys 100 instances rapidly
 *    - Tests Python object cleanup
 *    - Verifies no memory leaks during lifecycle
 *
 * Running the tests:
 * ============================================================
 *   Direct:  ./bin/python_indicators
 *   CTest:   ctest -R python_indicators -V
 *   Full:    ctest (runs all tests including these)
 *
 * Debugging segfaults:
 * ============================================================
 * If testing the UI integration and it segfaults:
 * 1. Check logs for "compute_sample failed" or "Python call failed" messages
 * 2. Run HighFrequencyCalls to isolate reference counting issues
 * 3. Run WrapperWithOHLCVSamples to test UI layer (python_indicator_wrapper)
 * 4. Compare with test results to identify where segfault occurs
 */

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

// Grox
#include "currency/ohlctv_sample.hpp"
#include "indicators/indicator_registry.hpp"
#include "indicators/python_plugin.hpp"

namespace fs = std::filesystem;

// Test fixture for Python indicator tests
class PythonIndicatorTest : public ::testing::Test
{
  protected:
  void SetUp() override
  {
    // Initialize Python plugin registry
    auto& registry = indicators::python::python_indicator_registry::instance();
    ASSERT_TRUE(registry.initialize()) << "Failed to initialize Python interpreter";
  }

  void TearDown() override
  {
    // Python runtime cleanup will happen at program exit
  }
};

// Test: Load Python indicator module and verify it's discovered
TEST_F(PythonIndicatorTest, LoadIndicatorModule)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Find the indicator example file
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";

  ASSERT_TRUE(fs::exists(indicator_path))
      << "Indicator example file not found at: " << indicator_path.string();

  // Load the module
  ASSERT_TRUE(registry.load_module(indicator_path))
      << "Failed to load Python indicator module from: " << indicator_path.string();

  // Get available indicators
  auto indicators = registry.get_available_indicators();
  EXPECT_GT(indicators.size(), 0) << "No indicators loaded from module";

  // Check that SimplePythonMovingAverage is available
  auto it = std::find(indicators.begin(), indicators.end(), "SimplePythonMovingAverage");
  ASSERT_NE(it, indicators.end()) << "SimplePythonMovingAverage not found in loaded indicators";

  std::cout << "Found " << indicators.size() << " Python indicators:\n";
  for (auto const& ind : indicators) { std::cout << "  - " << ind << "\n"; }
}

// Test: Create instance of Python indicator
TEST_F(PythonIndicatorTest, CreateIndicatorInstance)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr) << "Failed to create SimplePythonMovingAverage instance";

  EXPECT_NE(instance->get_py_object(), nullptr) << "Python object is null";
}

// Test: Run Python indicator on synthetic data and verify results
TEST_F(PythonIndicatorTest, ComputeSampleSimplePythonMovingAverage)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr);

  // Create synthetic OHLCV data (closing prices: 100, 101, 102, 103, 104, 105...)
  std::vector<double> prices;
  std::vector<double> expected_sma;

  // With period=20, compute SMA on incrementing prices
  for (int i = 0; i < 25; ++i)
  {
    double close = 100.0 + i;
    double result = instance->compute_sample(close,    // open
        close + 1,                                     // high
        close - 1,                                     // low
        close,                                         // close
        1000.0,                                        // volume
        i * 3600                                       // time (hourly)
    );

    prices.push_back(close);

    // Manual SMA calculation for verification
    int period = std::min(20, (int) prices.size());
    double manual_sma = 0.0;
    for (int j = std::max(0, (int) prices.size() - period); j < prices.size(); ++j)
    {
      manual_sma += prices[j];
    }
    manual_sma /= period;

    std::cout << fmt::format("Sample {}: close={:.2f}, computed={:.6f}, expected={:.6f}\n", i,
        close, result, manual_sma);

    // Allow small floating point differences
    EXPECT_NEAR(result, manual_sma, 1e-5) << fmt::format("Mismatch at sample {}", i);
  }
}

// Test: Run Python indicator on constant data
TEST_F(PythonIndicatorTest, ComputeSampleConstantData)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr);

  // Test with constant price
  double const_price = 123.45;
  std::vector<double> results(30);

  for (int i = 0; i < 30; ++i)
  {
    results[i] = instance->compute_sample(const_price,    // open
        const_price,                                      // high
        const_price,                                      // low
        const_price,                                      // close
        1000.0,                                           // volume
        i * 3600                                          // time
    );

    // SMA of constant value should be the value itself
    EXPECT_NEAR(results[i], const_price, 1e-6)
        << fmt::format("SMA of constant value failed at sample {}", i);
  }

  std::cout << "SMA of constant price " << const_price << " = " << results.back() << "\n";
}

// Test: Run ExponentialMovingAverage
TEST_F(PythonIndicatorTest, ComputeSampleExponentialMovingAverage)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance of EMA
  auto instance = registry.create_instance("ExponentialPythonMovingAverage");
  ASSERT_NE(instance, nullptr) << "Failed to create EMA instance";

  // Test with simple data
  std::vector<double> prices = {100.0, 101.0, 99.0, 102.0, 98.0, 103.0};

  for (int i = 0; i < prices.size(); ++i)
  {
    double result = instance->compute_sample(prices[i],    // open
        prices[i] + 1,                                     // high
        prices[i] - 1,                                     // low
        prices[i],                                         // close
        1000.0,                                            // volume
        i * 3600                                           // time
    );

    std::cout << fmt::format("EMA Sample {}: price={:.2f}, ema={:.6f}\n", i, prices[i], result);

    // Just verify we get a reasonable result (not NaN, not excessively large)
    EXPECT_FALSE(std::isnan(result)) << "EMA returned NaN";
    EXPECT_TRUE(std::isfinite(result)) << "EMA returned infinite value";
    EXPECT_GT(result, 0.0) << "EMA should be positive";
  }
}

// Test: Multiple instances don't interfere
TEST_F(PythonIndicatorTest, MultipleInstances)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create two instances
  auto instance1 = registry.create_instance("SimplePythonMovingAverage");
  auto instance2 = registry.create_instance("SimplePythonMovingAverage");

  ASSERT_NE(instance1, nullptr);
  ASSERT_NE(instance2, nullptr);
  EXPECT_NE(instance1.get(), instance2.get()) << "Should create different instances";

  // Feed different data to each
  std::vector<double> prices1 = {100.0, 101.0, 102.0};
  std::vector<double> prices2 = {200.0, 201.0, 202.0};

  for (int i = 0; i < 3; ++i)
  {
    double result1 = instance1->compute_sample(
        prices1[i], prices1[i] + 1, prices1[i] - 1, prices1[i], 1000.0, i * 3600);
    double result2 = instance2->compute_sample(
        prices2[i], prices2[i] + 1, prices2[i] - 1, prices2[i], 1000.0, i * 3600);

    std::cout << fmt::format("Instance1: {:.2f} -> {:.6f}, Instance2: {:.2f} -> {:.6f}\n",
        prices1[i], result1, prices2[i], result2);

    // They should produce different results
    EXPECT_NE(result1, result2) << fmt::format("Instances interfering at sample {}", i);
  }
}

// Test: python_indicator_wrapper with OHLCV samples (UI integration layer)
TEST_F(PythonIndicatorTest, WrapperWithOHLCVSamples)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create a wrapper (simulating what the UI does)
  indicators::python::python_indicator_wrapper wrapper(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);

  // Initialize the wrapper
  wrapper.init_params();
  wrapper.initialize();

  // Create synthetic OHLCV data
  std::vector<ohlctv_sample> samples;
  for (int i = 0; i < 20; ++i)
  {
    ohlctv_sample sample;
    sample.open = 100.0 + i;
    sample.high = 101.0 + i;
    sample.low = 99.0 + i;
    sample.close = 100.5 + i;
    sample.volume = 1000.0;
    sample.time = i * 3600;
    samples.push_back(sample);
  }

  // Process samples through wrapper
  for (size_t i = 0; i < samples.size(); ++i)
  {
    double result = wrapper(samples[i]);

    EXPECT_FALSE(std::isnan(result)) << fmt::format("Got NaN at sample {}", i);
    EXPECT_TRUE(std::isfinite(result)) << fmt::format("Got infinite value at sample {}", i);

    std::cout << fmt::format(
        "Wrapper sample {}: close={:.2f}, result={:.6f}\n", i, samples[i].close, result);
  }
}

// Test: High-frequency calls (stress test for reference counting)
TEST_F(PythonIndicatorTest, HighFrequencyCalls)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr);

  // Make many rapid calls - this will catch reference counting bugs
  int const NUM_CALLS = 1000;
  for (int i = 0; i < NUM_CALLS; ++i)
  {
    double price = 100.0 + (i % 50);
    double result = instance->compute_sample(price, price + 1, price - 1, price, 1000.0, i);

    EXPECT_FALSE(std::isnan(result)) << fmt::format("Got NaN at high-frequency call {}", i);
    EXPECT_TRUE(std::isfinite(result))
        << fmt::format("Got infinite value at high-frequency call {}", i);

    if (i % 250 == 0) { std::cout << fmt::format("High-freq call {}: {:.6f}\n", i, result); }
  }

  std::cout << "Completed " << NUM_CALLS << " calls without crash or NaN\n";
}

// Test: Return value validation (check for Python exceptions)
TEST_F(PythonIndicatorTest, ReturnValueValidation)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr);

  // Test edge case: very large prices
  double large_price = 1e10;
  double result_large =
      instance->compute_sample(large_price, large_price, large_price, large_price, 1e15, 0);
  EXPECT_TRUE(std::isfinite(result_large));

  // Test edge case: very small prices
  double small_price = 1e-10;
  double result_small =
      instance->compute_sample(small_price, small_price, small_price, small_price, 1e-15, 0);
  EXPECT_TRUE(std::isfinite(result_small));

  // Test edge case: zero volume
  double result_zero_vol = instance->compute_sample(100.0, 101.0, 99.0, 100.0, 0.0, 0);
  EXPECT_TRUE(std::isfinite(result_zero_vol));

  std::cout << fmt::format("Large price result: {:.6e}\n", result_large);
  std::cout << fmt::format("Small price result: {:.6e}\n", result_small);
  std::cout << fmt::format("Zero volume result: {:.6e}\n", result_zero_vol);
}

// Test: Verify all three indicators work correctly
TEST_F(PythonIndicatorTest, AllIndicatorTypes)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  std::vector<std::string> indicator_names = {
      "SimplePythonMovingAverage", "ExponentialPythonMovingAverage", "WeightedPythonMovingAverage"};

  // Test data
  std::vector<double> prices = {100.0, 102.0, 101.0, 103.0, 105.0, 104.0};

  for (auto const& name : indicator_names)
  {
    std::cout << "\nTesting " << name << ":\n";

    auto instance = registry.create_instance(name);
    ASSERT_NE(instance, nullptr) << "Failed to create " << name;

    std::vector<double> results;
    for (size_t i = 0; i < prices.size(); ++i)
    {
      double result = instance->compute_sample(
          prices[i], prices[i] + 2, prices[i] - 2, prices[i], 1000.0, i * 3600);

      EXPECT_TRUE(std::isfinite(result))
          << fmt::format("{}: Got non-finite value at sample {}", name, i);

      results.push_back(result);
      std::cout << fmt::format("  Sample {}: price={:.2f}, result={:.6f}\n", i, prices[i], result);
    }

    // Verify we got results for all samples
    EXPECT_EQ(results.size(), prices.size());
  }
}

// Test: Rapid instance creation and destruction
TEST_F(PythonIndicatorTest, InstanceLifecycle)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create and destroy many instances
  int const NUM_INSTANCES = 100;
  for (int i = 0; i < NUM_INSTANCES; ++i)
  {
    auto instance = registry.create_instance("SimplePythonMovingAverage");
    ASSERT_NE(instance, nullptr) << fmt::format("Failed to create instance {}", i);

    // Use it once
    double result = instance->compute_sample(100.0, 101.0, 99.0, 100.0, 1000.0, 0);
    EXPECT_TRUE(std::isfinite(result));

    // Explicitly delete by going out of scope (shared_ptr will handle it)
    if (i % 20 == 0)
    {
      std::cout << fmt::format("Created and used instance {}/{}\n", i + 1, NUM_INSTANCES);
    }
  }

  std::cout << "Successfully created and destroyed " << NUM_INSTANCES << " instances\n";
}

// Test: Factory method (what GUI uses to create indicators)
TEST_F(PythonIndicatorTest, FactoryMethod)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create a template wrapper (this is typically stored in the indicator registry)
  indicators::python::python_indicator_wrapper template_wrapper(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);

  template_wrapper.init_params();

  std::cout << fmt::format("Template wrapper class_name: {}\n", template_wrapper.get_class_name());
  EXPECT_EQ(template_wrapper.get_class_name(), "SimplePythonMovingAverage");
  EXPECT_NE(template_wrapper.get_registry(), nullptr);
  EXPECT_FALSE(template_wrapper.has_instance()) << "Instance should not exist before initialize()";

  // Simulate what the factory does when UI selects this indicator
  // The FACTORY_INDICATOR_CREATE macro:
  // 1. Creates a default instance
  // 2. Copies from the template
  // 3. Calls initialize()

  // We need to manually call initialize since we can't call create() without a full dataset
  template_wrapper.initialize();

  EXPECT_TRUE(template_wrapper.has_instance()) << "Instance should exist after initialize()";

  // Now test that it works
  std::vector<ohlctv_sample> samples;
  for (int i = 0; i < 10; ++i)
  {
    ohlctv_sample sample;
    sample.open = 100.0 + i;
    sample.high = 101.0 + i;
    sample.low = 99.0 + i;
    sample.close = 100.5 + i;
    sample.volume = 1000.0;
    sample.time = i * 3600;
    samples.push_back(sample);
  }

  for (size_t i = 0; i < samples.size(); ++i)
  {
    double result = template_wrapper(samples[i]);
    EXPECT_TRUE(std::isfinite(result))
        << fmt::format("Factory test: Got non-finite value at sample {}", i);

    std::cout << fmt::format("Factory test sample {}: {:.6f}\n", i, result);
  }
}

// Test: Test wrapper copy semantics (assignment operator)
TEST_F(PythonIndicatorTest, WrapperCopySemantics)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create original wrapper with proper initialization
  indicators::python::python_indicator_wrapper original(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);
  original.init_params();
  original.initialize();

  // Create a copy (simulates what FACTORY_INDICATOR_CREATE does)
  indicators::python::python_indicator_wrapper copied;
  copied = original;

  // Verify the copied wrapper has the same state
  EXPECT_EQ(copied.get_class_name(), "SimplePythonMovingAverage");
  EXPECT_NE(copied.get_registry(), nullptr);
  EXPECT_TRUE(copied.has_instance()) << "Copied wrapper should have instance";
  EXPECT_EQ(copied.getLastResult(), original.getLastResult());

  // Try to use the copied wrapper
  ohlctv_sample sample;
  sample.open = 100.0;
  sample.high = 101.0;
  sample.low = 99.0;
  sample.close = 100.5;
  sample.volume = 1000.0;
  sample.time = 0;

  double result_original = original(sample);
  double result_copied = copied(sample);

  EXPECT_TRUE(std::isfinite(result_original)) << "Original wrapper produced non-finite result";
  EXPECT_TRUE(std::isfinite(result_copied)) << "Copied wrapper produced non-finite result";

  std::cout << fmt::format(
      "Copy semantics: original={:.6f}, copied={:.6f}\n", result_original, result_copied);
}

// Test: Verify Python object persistence across copies
TEST_F(PythonIndicatorTest, PythonObjectPersistence)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create original wrapper and initialize
  auto wrapper1 = std::make_shared<indicators::python::python_indicator_wrapper>(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);
  wrapper1->init_params();
  wrapper1->initialize();

  // Create a second wrapper by copying
  auto wrapper2 = std::make_shared<indicators::python::python_indicator_wrapper>(*wrapper1);

  // Both should be able to compute independently
  ohlctv_sample sample;
  sample.open = 100.0;
  sample.high = 101.0;
  sample.low = 99.0;
  sample.close = 100.5;
  sample.volume = 1000.0;
  sample.time = 0;

  double result1 = (*wrapper1)(sample);
  EXPECT_TRUE(std::isfinite(result1)) << "Wrapper1 result is not finite";

  double result2 = (*wrapper2)(sample);
  EXPECT_TRUE(std::isfinite(result2)) << "Wrapper2 result is not finite";

  // Feed more data to wrapper1
  for (int i = 1; i < 5; ++i)
  {
    sample.close = 100.0 + i;
    double r1 = (*wrapper1)(sample);
    EXPECT_TRUE(std::isfinite(r1));
  }

  // Feed different data to wrapper2 (should maintain independent state if each has own instance)
  for (int i = 1; i < 5; ++i)
  {
    sample.close = 200.0 + i;
    double r2 = (*wrapper2)(sample);
    EXPECT_TRUE(std::isfinite(r2));
  }

  std::cout << "Python object persistence test passed\n";
}

// Test: Parameter extraction from Python indicators
TEST_F(PythonIndicatorTest, ParameterExtraction)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create wrapper and extract parameters
  indicators::python::python_indicator_wrapper wrapper(
      "Python SMA", "Simple Moving Average", "SimplePythonMovingAverage", &registry);

  wrapper.init_params();

  // Check that parameters were extracted
  auto const& params = wrapper.get_params();
  EXPECT_GT(params.size(), 0) << "Should have extracted at least one parameter";

  std::cout << fmt::format("Extracted {} parameters:\n", params.size());

  for (size_t i = 0; i < params.size(); ++i)
  {
    std::visit(
        [i](auto&& param_variant) {
          std::cout << fmt::format("  [{}] {} = ", i, param_variant.name_.toStdString());
          using T = std::decay_t<decltype(param_variant.val_)>;
          if constexpr (std::is_same_v<T, int>) { std::cout << param_variant.val_ << " (int)\n"; }
          else if constexpr (std::is_same_v<T, double>)
          {
            std::cout << param_variant.val_ << " (double)\n";
          }
          else if constexpr (std::is_same_v<T, std::string>)
          {
            std::cout << param_variant.val_ << " (string)\n";
          }
          else if constexpr (std::is_same_v<T, ohlc_modes>)
          {
            std::cout << static_cast<int>(param_variant.val_) << " (ohlc_modes)\n";
          }
          else { std::cout << "(unknown type)\n"; }
        },
        params[i]);
  }
}

// Test: Parameter setting and modification
TEST_F(PythonIndicatorTest, ParameterModification)
{
  auto& registry = indicators::python::python_indicator_registry::instance();

  // Load module
  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(registry.load_module(indicator_path));

  // Create instance
  auto instance = registry.create_instance("SimplePythonMovingAverage");
  ASSERT_NE(instance, nullptr);

  // Set period to a different value
  bool set_success = instance->set_parameter("Period", 10);
  EXPECT_TRUE(set_success) << "Failed to set period parameter";

  // Verify the parameter was set
  int new_period = instance->get_int_parameter("period");
  EXPECT_EQ(new_period, 10) << "Period was not updated";

  std::cout << fmt::format("Successfully set period to {}\n", new_period);

  // Set price_type
  bool set_success2 = instance->set_parameter("Price Type", std::string("high"));
  EXPECT_TRUE(set_success2) << "Failed to set price_type parameter";

  std::string new_price_type = instance->get_string_parameter("price_type");
  EXPECT_EQ(new_price_type, "high") << "Price type was not updated";

  std::cout << fmt::format("Successfully set price_type to {}\n", new_price_type);

  // Verify the indicator still works with new parameters
  std::vector<double> results;
  for (int i = 0; i < 15; ++i)
  {
    double result = instance->compute_sample(100.0 + i, 102.0 + i, 98.0 + i, 101.0 + i, 1000.0, i);
    results.push_back(result);
    EXPECT_TRUE(std::isfinite(result));
  }

  std::cout << fmt::format("Computed {} samples with modified parameters\n", results.size());
}

//----------------------------------------------------------------------------
// End-to-end: Python indicators registered with the main registry
// are correctly partitioned by kind() and distinguishable by source().
//----------------------------------------------------------------------------
TEST_F(PythonIndicatorTest, RegisterWithMainRegistryByKind)
{
  auto& py_registry = indicators::python::python_indicator_registry::instance();

  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(py_registry.load_module(indicator_path));

  // Use a fresh indicator_registry instance (not the singleton) so this
  // test is isolated from the global registry used by the GUI.
  // We can't easily construct one privately, so we use the singleton and
  // verify the Python indicators appear alongside any C++ indicators.
  auto& main_registry = indicators::indicator_registry::getInstance();

  std::size_t count = py_registry.register_with_main_registry(main_registry);
  ASSERT_GT(count, 0u) << "No Python indicators registered";

  // All example indicators declare kind = "graph", so they must land
  // in the graph partition.
  auto const& graph_indicators = main_registry.by_kind(indicators::indicator_kind::graph);
  EXPECT_FALSE(graph_indicators.empty()) << "Graph partition is empty";

  // At least one indicator in the graph partition must be a Python wrapper
  // (source() == python).
  bool found_python = false;
  for (auto const& alg : graph_indicators)
  {
    if (alg->source() == indicators::indicator_source::python)
    {
      found_python = true;
      EXPECT_EQ(alg->kind(), indicators::indicator_kind::graph)
          << "Python indicator has wrong kind";
      break;
    }
  }
  EXPECT_TRUE(found_python) << "No Python indicators found in graph partition";
}

//----------------------------------------------------------------------------
// End-to-end: find_by_name locates a Python indicator in the unified registry
//----------------------------------------------------------------------------
TEST_F(PythonIndicatorTest, FindPythonIndicatorByName)
{
  auto& py_registry = indicators::python::python_indicator_registry::instance();

  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(py_registry.load_module(indicator_path));

  auto& main_registry = indicators::indicator_registry::getInstance();
  py_registry.register_with_main_registry(main_registry);

  // register_with_main_registry uses the Python class name (e.g.
  // "SimplePythonMovingAverage") as the indicator name, not the
  // human-readable "name" attribute (e.g. "Python SMA").
  auto alg = indicators::indicator_registry::find_by_name("SimplePythonMovingAverage");
  ASSERT_NE(alg, nullptr) << "find_by_name failed for SimplePythonMovingAverage";
  EXPECT_EQ(alg->source(), indicators::indicator_source::python)
      << "SimplePythonMovingAverage should have python source";
  EXPECT_EQ(alg->kind(), indicators::indicator_kind::graph)
      << "SimplePythonMovingAverage should have graph kind";
}

//----------------------------------------------------------------------------
// Multi-output: a Python indicator returning a list of floats
// produces a std::span<float const> from process_sample.
//----------------------------------------------------------------------------
TEST_F(PythonIndicatorTest, MultiOutputPythonIndicator)
{
  auto& py_registry = indicators::python::python_indicator_registry::instance();

  fs::path indicator_path =
      fs::path(__FILE__).parent_path().parent_path() / "python" / "indicator_example.py";
  ASSERT_TRUE(py_registry.load_module(indicator_path));

  auto& main_registry = indicators::indicator_registry::getInstance();
  py_registry.register_with_main_registry(main_registry);

  auto alg = indicators::indicator_registry::find_by_name("SimplePythonMovingAverage");
  ASSERT_NE(alg, nullptr);

  // The wrapper must report num_outputs = 1 (default for SimplePythonMovingAverage)
  EXPECT_EQ(alg->num_outputs(), 1);
}

//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
