// Verification of zero memory and object layout overhead.

#include <type_traits>
#include "histo/histo.hpp"
#include "test_framework.hpp"

using namespace libhisto;

// Compile-time static assertions ensuring exact pointer sizes
static_assert(sizeof(HistogramView) == sizeof(void*),
              "HistogramView must have zero memory overhead beyond a single pointer");
static_assert(sizeof(Histogram) == sizeof(void*),
              "Histogram must have zero memory overhead beyond a single pointer");
static_assert(sizeof(Histogram2DView) == sizeof(void*),
              "Histogram2DView must have zero memory overhead beyond a single pointer");
static_assert(sizeof(Histogram2D) == sizeof(void*),
              "Histogram2D must have zero memory overhead beyond a single pointer");
static_assert(sizeof(SketchView) == sizeof(void*),
              "SketchView must have zero memory overhead beyond a single pointer");
static_assert(sizeof(DDSketch) == sizeof(void*),
              "DDSketch must have zero memory overhead beyond a single pointer");
static_assert(sizeof(KDE) == sizeof(void*),
              "KDE must have zero memory overhead beyond a single pointer");
static_assert(sizeof(Status) == sizeof(int32_t),
              "Status must be exact size of a 32-bit status code");

// Standard layout and move constructible checks
static_assert(std::is_standard_layout_v<HistogramView>,
              "HistogramView must be standard layout");
static_assert(std::is_nothrow_move_constructible_v<Histogram>,
              "Histogram must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable_v<Histogram>,
              "Histogram must be nothrow move assignable");

HISTO_TEST(ZeroOverheadTest, MemorySizeChecks) {
  EXPECT_EQ(sizeof(Histogram), sizeof(void*));
  EXPECT_EQ(sizeof(HistogramView), sizeof(void*));
  EXPECT_EQ(sizeof(Histogram2D), sizeof(void*));
  EXPECT_EQ(sizeof(DDSketch), sizeof(void*));
}
