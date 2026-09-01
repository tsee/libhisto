// Unit tests for range-based for loops and bin iterators.

#include <numeric>
#include <vector>
#include "histo/histogram.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(IteratorTest, RangeForLoop) {
  Histogram h(5, 0.0, 50.0);
  h.Fill(5.0);   // Bin 0
  h.Fill(15.0);  // Bin 1
  h.Fill(25.0);  // Bin 2
  h.Fill(35.0);  // Bin 3
  h.Fill(45.0);  // Bin 4

  uint32_t count = 0;
  double total = 0.0;
  for (const Bin& bin : h) {
    EXPECT_EQ(bin.index, count);
    EXPECT_NEAR(bin.lower_edge, count * 10.0, 1e-9);
    EXPECT_NEAR(bin.upper_edge, (count + 1) * 10.0, 1e-9);
    EXPECT_NEAR(bin.width(), 10.0, 1e-9);
    EXPECT_NEAR(bin.center(), count * 10.0 + 5.0, 1e-9);
    EXPECT_NEAR(bin.content, 1.0, 1e-9);
    total += bin.content;
    count++;
  }

  EXPECT_EQ(count, 5u);
  EXPECT_NEAR(total, 5.0, 1e-9);
}

HISTO_TEST(IteratorTest, EmptyHistogramIteration) {
  Histogram h(10, 0.0, 10.0);
  uint32_t count = 0;
  for (const auto& bin : h) {
    EXPECT_NEAR(bin.content, 0.0, 1e-9);
    count++;
  }
  EXPECT_EQ(count, 10u);
}
