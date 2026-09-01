// Unit tests for histogram arithmetic, scaling, and merging.

#include "histo/histogram.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(ArithmeticTest, ScaleOperator) {
  Histogram h(10, 0.0, 10.0);
  h.Fill(3.5);
  h.Fill(7.5);

  EXPECT_NEAR(h.total_weight(), 2.0, 1e-9);

  // Scale by 2.5
  h *= 2.5;
  EXPECT_NEAR(h.total_weight(), 5.0, 1e-9);
  EXPECT_NEAR(h.bin_content(3), 2.5, 1e-9);
  EXPECT_NEAR(h.bin_content(7), 2.5, 1e-9);
}

HISTO_TEST(ArithmeticTest, MergeOperator) {
  Histogram h1(10, 0.0, 10.0, HISTO_FLAG_EXACT_MOMENTS);
  h1.Fill(2.0);

  Histogram h2(10, 0.0, 10.0, HISTO_FLAG_EXACT_MOMENTS);
  h2.Fill(4.0);

  // Merge h2 into h1 using operator+=
  h1 += h2;
  EXPECT_EQ(h1.n_entries(), 2u);
  EXPECT_NEAR(h1.total_weight(), 2.0, 1e-9);
  EXPECT_NEAR(h1.mean(), 3.0, 1e-9);

  // Merging incompatible histograms safely returns error
  Histogram h_incompat(5, 0.0, 10.0);
  Status s = h1.Merge(h_incompat);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(static_cast<int32_t>(s.code()), static_cast<int32_t>(StatusCode::kIncompatible));
}

HISTO_TEST(ArithmeticTest, ResetMethod) {
  Histogram h(10, 0.0, 10.0);
  h.Fill(1.0);
  h.Fill(2.0);

  EXPECT_EQ(h.n_entries(), 2u);
  EXPECT_STATUS_OK(h.Reset());
  EXPECT_EQ(h.n_entries(), 0u);
  EXPECT_NEAR(h.total_weight(), 0.0, 1e-9);
}
