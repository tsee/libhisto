// Unit tests for move semantics, ownership transfer, and cloning.

#include <utility>
#include "histo/histogram.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(MoveTest, MoveConstructor) {
  Histogram h1(10, 0.0, 10.0);
  h1.Fill(5.0);

  // Move construct
  Histogram h2(std::move(h1));
  EXPECT_TRUE(h2.is_valid());
  EXPECT_EQ(h2.n_entries(), 1u);
  EXPECT_NEAR(h2.total_weight(), 1.0, 1e-9);

  // Moved-from object is safely invalidated
  EXPECT_FALSE(h1.is_valid());
  EXPECT_FALSE(h1.Fill(1.0).ok());
}

HISTO_TEST(MoveTest, MoveAssignment) {
  Histogram h1(10, 0.0, 10.0);
  h1.Fill(3.0);

  Histogram h2(20, 0.0, 20.0);
  h2.Fill(1.0);
  h2.Fill(2.0);

  // Move assign
  h2 = std::move(h1);
  EXPECT_TRUE(h2.is_valid());
  EXPECT_EQ(h2.nbins(), 10u);
  EXPECT_EQ(h2.n_entries(), 1u);

  // Moved-from object
  EXPECT_FALSE(h1.is_valid());
}

HISTO_TEST(MoveTest, CloneDeepCopy) {
  Histogram h1(10, 0.0, 10.0);
  h1.Fill(4.0);

  // Clone exact copy
  Histogram h2 = h1.Clone();
  EXPECT_TRUE(h2.is_valid());
  EXPECT_EQ(h2.n_entries(), 1u);

  // Modifying h2 does not affect h1
  h2.Fill(4.0);
  EXPECT_EQ(h1.n_entries(), 1u);
  EXPECT_EQ(h2.n_entries(), 2u);

  // Clone empty schema copy
  Histogram h_empty = h1.Clone(/*empty=*/true);
  EXPECT_TRUE(h_empty.is_valid());
  EXPECT_EQ(h_empty.nbins(), 10u);
  EXPECT_EQ(h_empty.n_entries(), 0u);
}
