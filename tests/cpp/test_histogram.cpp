// Unit tests for libhisto::Histogram 1D construction, ingestion, and query methods.

#include <vector>
#include "histo/histogram.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(HistogramTest, UniformConstruction) {
  Histogram h(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS | HISTO_FLAG_TRACK_SUMW2);
  EXPECT_TRUE(h.is_valid());
  EXPECT_TRUE(static_cast<bool>(h));
  EXPECT_EQ(h.nbins(), 10u);
  EXPECT_NEAR(h.min(), 0.0, 1e-9);
  EXPECT_NEAR(h.max(), 100.0, 1e-9);
  EXPECT_EQ(static_cast<int>(h.bin_type()), static_cast<int>(HISTO_BIN_UNIFORM));
  EXPECT_NEAR(h.total_weight(), 0.0, 1e-9);
  EXPECT_EQ(h.n_entries(), 0u);
}

HISTO_TEST(HistogramTest, InvalidParametersSafe) {
  // Invalid min >= max
  Histogram h_bad_range(10, 100.0, 0.0);
  EXPECT_FALSE(h_bad_range.is_valid());
  EXPECT_FALSE(static_cast<bool>(h_bad_range));
  EXPECT_EQ(h_bad_range.nbins(), 0u);
  EXPECT_FALSE(h_bad_range.Fill(5.0).ok());

  // Factory failure returns error Status
  auto res_bad = Histogram::Uniform(0, 0.0, 10.0);
  EXPECT_FALSE(res_bad.ok());
}

HISTO_TEST(HistogramTest, VariableConstruction) {
  Histogram h({0.0, 1.0, 5.0, 10.0, 50.0, 100.0});
  EXPECT_TRUE(h.is_valid());
  EXPECT_EQ(h.nbins(), 5u);
  EXPECT_EQ(static_cast<int>(h.bin_type()), static_cast<int>(HISTO_BIN_VARIABLE));
  EXPECT_NEAR(h.bin_low_edge(0), 0.0, 1e-9);
  EXPECT_NEAR(h.bin_high_edge(0), 1.0, 1e-9);
  EXPECT_NEAR(h.bin_low_edge(1), 1.0, 1e-9);
  EXPECT_NEAR(h.bin_high_edge(1), 5.0, 1e-9);
}

HISTO_TEST(HistogramTest, SingleFillAndWeightedFill) {
  Histogram h(10, 0.0, 10.0, HISTO_FLAG_EXACT_MOMENTS);
  EXPECT_STATUS_OK(h.Fill(2.5));
  EXPECT_STATUS_OK(h.Fill(2.5, 3.0));

  EXPECT_EQ(h.n_entries(), 2u);
  EXPECT_NEAR(h.total_weight(), 4.0, 1e-9);
  EXPECT_NEAR(h.bin_content(2), 4.0, 1e-9);
  EXPECT_NEAR(h.mean(), 2.5, 1e-9);
}

HISTO_TEST(HistogramTest, BatchIngestionAndSpan) {
  Histogram h(10, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);

  // Ingestion from std::vector
  std::vector<double> data = {10.0, 20.0, 30.0, 40.0, 50.0};
  EXPECT_STATUS_OK(h.Fill(data));
  EXPECT_EQ(h.n_entries(), 5u);
  EXPECT_NEAR(h.total_weight(), 5.0, 1e-9);
  EXPECT_NEAR(h.mean(), 30.0, 1e-9);

  // Ingestion from initializer_list
  EXPECT_STATUS_OK(h.Fill({60.0, 70.0, 80.0, 90.0}));
  EXPECT_EQ(h.n_entries(), 9u);
}

HISTO_TEST(HistogramTest, OutOfBoundsAndSpecialValues) {
  Histogram h(10, 0.0, 10.0);
  h.Fill(-5.0); // Underflow
  h.Fill(15.0); // Overflow

  EXPECT_NEAR(h.underflow_weight(), 1.0, 1e-9);
  EXPECT_NEAR(h.overflow_weight(), 1.0, 1e-9);

  // NaN / Inf input handling
  Status s_nan = h.Fill(NAN);
  EXPECT_FALSE(s_nan.ok());
  EXPECT_EQ(h.nan_count(), 1u);
}

HISTO_TEST(HistogramTest, StatisticsAndQuantiles) {
  Histogram h(100, 0.0, 100.0, HISTO_FLAG_EXACT_MOMENTS);
  for (int i = 0; i < 100; ++i) {
    h.Fill(i + 0.5);
  }

  auto stats_res = h.stats();
  EXPECT_TRUE(stats_res.ok());
  EXPECT_EQ(stats_res->n_entries, 100u);
  EXPECT_NEAR(stats_res->mean, 50.0, 1e-9);

  auto q50 = h.quantile(0.50);
  EXPECT_TRUE(q50.ok());
  EXPECT_NEAR(*q50, 50.0, 1.0);
}
