// Unit tests for DDSketch dynamic quantile sketches in C++.

#include "histo/sketch.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(SketchTest, ConstructionAndInsert) {
  DDSketch s(0.01);
  EXPECT_TRUE(s.is_valid());
  EXPECT_EQ(s.count(), 0u);

  EXPECT_STATUS_OK(s.Insert(10.0));
  EXPECT_STATUS_OK(s.Insert(20.0));
  EXPECT_STATUS_OK(s.Insert(30.0));
  EXPECT_STATUS_OK(s.Insert(40.0));
  EXPECT_STATUS_OK(s.Insert(50.0));

  EXPECT_EQ(s.count(), 5u);
  EXPECT_NEAR(s.min(), 10.0, 1e-9);
  EXPECT_NEAR(s.max(), 50.0, 1e-9);
  EXPECT_NEAR(s.total_weight(), 5.0, 1e-9);

  auto q50 = s.quantile(0.5);
  EXPECT_TRUE(q50.ok());
  EXPECT_NEAR(*q50, 30.0, 1.0);
}

HISTO_TEST(SketchTest, MergeAndReset) {
  DDSketch s1(0.01);
  s1.Insert(100.0);

  DDSketch s2(0.01);
  s2.Insert(200.0);

  s1 += s2;
  EXPECT_EQ(s1.count(), 2u);

  EXPECT_STATUS_OK(s1.Reset());
  EXPECT_EQ(s1.count(), 0u);
}
