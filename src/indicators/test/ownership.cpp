// Unit tests for indicator_base output ownership semantics.
//
// Verifies that output data is deleted exactly once across three scenarios:
//   1. Destroyed without release_outputs() → exactly N deletions
//   2. Destroyed after release_outputs()   → zero deletions (Qwt would own)
//   3. Copied: original + copy destroyed    → exactly N deletions (not 2N)

#include <atomic>
#include <memory>
#include <vector>
//
#include <gtest/gtest.h>
//
#include "data/ohlc_data_resolutions.hpp"
#include "data/ohlc_dataset.hpp"
#include "data/ohlc_dataset_view.hpp"
#include "data/timebased_chart_data.hpp"
#include "indicators/algorithm_base.hpp"
#include "indicators/indicator_base.hpp"
#include "indicators/indicator_types.hpp"

namespace {

  /// Subclass of point_chart_data that increments a static atomic on
  /// destruction. Requires timebased_chart_data's destructor to be virtual.
  class counting_output : public point_chart_data
  {
public:
    static std::atomic<int> delete_count;

    counting_output(candle_res res)
      : point_chart_data(res)
    {
    }

    ~counting_output() override { ++delete_count; }
  };

  std::atomic<int> counting_output::delete_count{0};

}    // namespace

namespace indicators {

  /// Minimal indicator that overrides create_outputs() to use counting_output.
  /// num_outputs() returns 2 so we can verify per-output deletion.
  class test_indicator : public indicator_base
  {
public:
    FACTORY_INDICATOR_V2(test_indicator)

    test_indicator()
      : indicator_base(
            "test", "test indicator", {overlay_type::no_overlay, overlay_type::no_overlay})
    {
    }

    int num_outputs() const override { return 2; }

    void init_params() override {}

    void initialize() override {}

    sample_result process_sample(market_sample const&) override { return 0.0; }

    /// Override to create counting_output instances instead of point_chart_data.
    void create_outputs(std::shared_ptr<ohlc_dataset_view>) override
    {
      candle_res const res{1000.0, 1000.0, "1m"};
      for (int i = 0; i < num_outputs(); ++i)
      {
        auto raw = new counting_output(res);
        out_datasets_.push_back(std::shared_ptr<output_type>(raw, [](output_type*) {}));
      }
    }
  };

}    // namespace indicators

// ----------------------------------------------------------------------------
// Scenario 1: Destroy without release_outputs()
// ~indicator_base() should delete exactly N (2) outputs.
// ----------------------------------------------------------------------------
TEST(indicator_ownership, destroy_without_release_deletes_once)
{
  counting_output::delete_count = 0;

  {
    auto alg = std::make_shared<indicators::test_indicator>();
    alg->create_outputs(nullptr);
    ASSERT_EQ(alg->get_outputs().size(), 2u);
    // Not released → ~indicator_base() should delete both
  }

  EXPECT_EQ(counting_output::delete_count.load(), 2)
      << "Expected 2 deletions (one per output), got " << counting_output::delete_count.load();
}

// ----------------------------------------------------------------------------
// Scenario 2: Destroy after release_outputs()
// ~indicator_base() should delete zero outputs (Qwt would own them).
// ----------------------------------------------------------------------------
TEST(indicator_ownership, destroy_after_release_deletes_zero)
{
  counting_output::delete_count = 0;

  {
    auto alg = std::make_shared<indicators::test_indicator>();
    alg->create_outputs(nullptr);
    ASSERT_EQ(alg->get_outputs().size(), 2u);

    // Manually track raw pointers so we can clean up after the test
    std::vector<counting_output*> raws;
    for (auto& d : alg->get_outputs()) { raws.push_back(static_cast<counting_output*>(d.get())); }

    alg->release_outputs();
    // Released → ~indicator_base() should NOT delete
  }

  EXPECT_EQ(counting_output::delete_count.load(), 0)
      << "Expected 0 deletions (Qwt owns), got " << counting_output::delete_count.load();

  // Clean up the leaked outputs (in real code Qwt curves would own them)
  // We can't get the pointers back after the indicator is destroyed, so
  // this test intentionally leaks. The delete_count assertion is the test.
}

// ----------------------------------------------------------------------------
// Scenario 3: Copy then destroy both
// Only the last surviving copy (unique) should delete. Total: exactly N.
// ----------------------------------------------------------------------------
TEST(indicator_ownership, copy_then_destroy_deletes_once)
{
  counting_output::delete_count = 0;

  {
    auto alg = std::make_shared<indicators::test_indicator>();
    alg->create_outputs(nullptr);
    ASSERT_EQ(alg->get_outputs().size(), 2u);

    // Copy the indicator (as FACTORY_INDICATOR_V2 does)
    auto copy = std::make_shared<indicators::test_indicator>(*alg);

    // Destroy the copy first — not unique, should NOT delete
    copy.reset();
    EXPECT_EQ(counting_output::delete_count.load(), 0)
        << "Copy destruction should not delete (refcount was 2)";

    // Destroy the original — now unique, should delete both
    alg.reset();
    EXPECT_EQ(counting_output::delete_count.load(), 2)
        << "Original destruction should delete exactly 2, got "
        << counting_output::delete_count.load();
  }
}

// ----------------------------------------------------------------------------
// Scenario 4: Copy, release on original, destroy both
// release_outputs() sets the flag on the original. The copy doesn't have
// the flag set, but since the copy is not unique, it won't delete.
// The original has the flag set, so it won't delete either.
// This simulates the real-world case where the Qt thread creates curves
// (release_outputs) and a temporary copy on the pika thread is destroyed.
// ----------------------------------------------------------------------------
TEST(indicator_ownership, copy_released_then_destroy_deletes_zero)
{
  counting_output::delete_count = 0;

  {
    auto alg = std::make_shared<indicators::test_indicator>();
    alg->create_outputs(nullptr);
    ASSERT_EQ(alg->get_outputs().size(), 2u);

    // Copy (as the sender pipeline does)
    auto copy = std::make_shared<indicators::test_indicator>(*alg);

    // Qt thread: release outputs on the original
    alg->release_outputs();

    // Pika thread: copy destroyed (not unique → no delete)
    copy.reset();
    EXPECT_EQ(counting_output::delete_count.load(), 0) << "Copy destruction should not delete";

    // Qt thread: original destroyed (released → no delete)
    alg.reset();
    EXPECT_EQ(counting_output::delete_count.load(), 0)
        << "Original destruction should not delete (released), got "
        << counting_output::delete_count.load();
  }

  // Note: outputs are leaked here because no Qwt curve owns them in the test.
  // In production, Qwt curves created via setData() would own and free them.
}

// ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
