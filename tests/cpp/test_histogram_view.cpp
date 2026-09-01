// Unit tests for libhisto::HistogramView zero-copy C interoperability.

#include "histo/histo.h"
#include "histo/histogram.hpp"
#include "histo/histogram_view.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(HistogramViewTest, WrapRawPointer) {
  histo_t* raw = histo_create_uniform(20, -10.0, 10.0, HISTO_FLAG_EXACT_MOMENTS);
  histo_fill(raw, 0.0);
  histo_fill(raw, 5.0);

  // Wrap without taking ownership
  HistogramView view(raw);
  EXPECT_TRUE(view.is_valid());
  EXPECT_EQ(view.nbins(), 20u);
  EXPECT_NEAR(view.min(), -10.0, 1e-9);
  EXPECT_NEAR(view.max(), 10.0, 1e-9);
  EXPECT_EQ(view.n_entries(), 2u);
  EXPECT_NEAR(view.mean(), 2.5, 1e-9);

  // Destroy raw C handle
  histo_destroy(raw);
}

HISTO_TEST(HistogramViewTest, NullSafety) {
  HistogramView null_view(nullptr);
  EXPECT_FALSE(null_view.is_valid());
  EXPECT_FALSE(static_cast<bool>(null_view));
  EXPECT_EQ(null_view.nbins(), 0u);
  EXPECT_NEAR(null_view.mean(), 0.0, 1e-9);
  EXPECT_FALSE(null_view.stats().ok());
  EXPECT_FALSE(null_view.quantile(0.5).ok());
}

HISTO_TEST(HistogramViewTest, OwningHistogramConversion) {
  Histogram h(10, 0.0, 10.0);
  h.Fill(3.0);

  // Pass to function accepting const HistogramView&
  auto inspect = [](const HistogramView& v) {
    EXPECT_TRUE(v.is_valid());
    EXPECT_EQ(v.nbins(), 10u);
    EXPECT_NEAR(v.total_weight(), 1.0, 1e-9);
  };

  inspect(h);
}
