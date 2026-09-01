// Unit tests for KDE continuous density estimation and curve fitting in C++.

#include <vector>
#include "histo/fit.hpp"
#include "histo/histogram.hpp"
#include "histo/kde.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(KDETest, EvaluationFromSamples) {
  std::vector<double> samples = {-1.0, 0.0, 1.0, 2.0, 3.0};
  KDE kde(samples);
  EXPECT_TRUE(kde.is_valid());
  EXPECT_TRUE(kde.bandwidth() > 0.0);

  double pdf = kde.EvalPdf(1.0);
  EXPECT_TRUE(pdf > 0.0);

  double cdf = kde.EvalCdf(1.0);
  EXPECT_TRUE(cdf >= 0.0 && cdf <= 1.0);

  auto sample_res = kde.SampleN(5, 42);
  EXPECT_TRUE(sample_res.ok());
  EXPECT_EQ(sample_res->size(), 5u);
}

HISTO_TEST(FitTest, GaussianFit) {
  Histogram h(50, -5.0, 5.0);
  // Fill Gaussian-like shape
  for (int i = 0; i < 1000; ++i) {
    h.Fill(0.0);
  }
  for (int i = 0; i < 500; ++i) {
    h.Fill(1.0);
    h.Fill(-1.0);
  }

  FitOptions opts = DefaultFitOptions();
  auto fit_res = Fit(h, HISTO_FIT_MODEL_GAUSSIAN, {}, &opts);
  EXPECT_TRUE(fit_res.ok());
  EXPECT_TRUE(fit_res->converged());
  EXPECT_EQ(fit_res->num_params(), 3u);
  EXPECT_EQ(fit_res->params().size(), 3u);
}
