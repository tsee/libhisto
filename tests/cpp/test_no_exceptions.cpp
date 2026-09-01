// Verification that the C++ wrapper compiles and runs under -fno-exceptions.

#include "histo/histo.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(NoExceptionsTest, SafeOperationsUnderNoExceptions) {
  // Test that all constructs operate without throwing
  Histogram h(10, 0.0, 10.0);
  EXPECT_TRUE(h.is_valid());

  EXPECT_STATUS_OK(h.Fill(5.0));
  EXPECT_STATUS_OK(h.Reset());

  Result<Histogram> res = Histogram::Uniform(10, 0.0, 10.0);
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(res->nbins(), 10u);
}
