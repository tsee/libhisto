// Unit tests for 2D histograms, bivariate moments, and projections.

#include <vector>
#include "histo/histogram2d.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(Histogram2DTest, UniformUniformConstruction) {
  Histogram2D h2(10, 0.0, 10.0, 20, 0.0, 20.0, HISTO_FLAG_EXACT_MOMENTS);
  EXPECT_TRUE(h2.is_valid());
  EXPECT_EQ(h2.nx(), 10u);
  EXPECT_EQ(h2.ny(), 20u);
  EXPECT_EQ(h2.total_bins(), 200u);
}

HISTO_TEST(Histogram2DTest, FillAndProjections) {
  Histogram2D h2(10, 0.0, 10.0, 10, 0.0, 10.0);
  EXPECT_STATUS_OK(h2.Fill(2.5, 4.5));
  EXPECT_STATUS_OK(h2.Fill(2.5, 6.5));

  EXPECT_EQ(h2.n_entries(), 2u);
  EXPECT_NEAR(h2.total_weight(), 2.0, 1e-9);

  // Projections
  auto px = h2.ProjectX();
  EXPECT_TRUE(px.ok());
  EXPECT_EQ(px->nbins(), 10u);
  EXPECT_NEAR(px->total_weight(), 2.0, 1e-9);
  EXPECT_NEAR(px->mean(), 2.5, 1e-9);

  auto py = h2.ProjectY();
  EXPECT_TRUE(py.ok());
  EXPECT_EQ(py->nbins(), 10u);
  EXPECT_NEAR(py->mean(), 5.5, 1e-9);
}

HISTO_TEST(Histogram2DTest, VariableAxes) {
  std::vector<double> x_edges = {0.0, 1.0, 5.0, 10.0};
  std::vector<double> y_edges = {0.0, 2.0, 4.0, 8.0, 16.0};

  Histogram2D h_var(x_edges, y_edges);
  EXPECT_TRUE(h_var.is_valid());
  EXPECT_EQ(h_var.nx(), 3u);
  EXPECT_EQ(h_var.ny(), 4u);
  EXPECT_EQ(h_var.total_bins(), 12u);
}
