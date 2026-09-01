// Unit tests for libhisto::Status, StatusCode, Result<T>, and ASSIGN_OR_RETURN.

#include "histo/status.hpp"
#include "test_framework.hpp"

using namespace libhisto;

HISTO_TEST(StatusTest, OkStatus) {
  Status s = OkStatus();
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(static_cast<bool>(s));
  EXPECT_EQ(static_cast<int32_t>(s.code()), static_cast<int32_t>(StatusCode::kOk));
  EXPECT_EQ(s.c_status(), HISTO_OK);
}

HISTO_TEST(StatusTest, ErrorStatus) {
  Status s(StatusCode::kInvalidArgument);
  EXPECT_FALSE(s.ok());
  EXPECT_FALSE(static_cast<bool>(s));
  EXPECT_EQ(static_cast<int32_t>(s.code()), static_cast<int32_t>(StatusCode::kInvalidArgument));
  EXPECT_EQ(s.c_status(), HISTO_ERR_INVALID_ARG);
  EXPECT_TRUE(s.message() != nullptr);
}

HISTO_TEST(ResultTest, SuccessValue) {
  Result<int> res(42);
  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(static_cast<bool>(res));
  EXPECT_EQ(res.value(), 42);
  EXPECT_EQ(*res, 42);
  EXPECT_EQ(res.value_or(100), 42);
}

HISTO_TEST(ResultTest, ErrorStatus) {
  Result<int> res(StatusCode::kOutOfMemory);
  EXPECT_FALSE(res.ok());
  EXPECT_FALSE(static_cast<bool>(res));
  EXPECT_EQ(static_cast<int32_t>(res.status().code()), static_cast<int32_t>(StatusCode::kOutOfMemory));
  EXPECT_EQ(res.value_or(100), 100);
}

static Result<int> DoubleIfPositive(int x) {
  if (x < 0) return Status(StatusCode::kInvalidArgument);
  return x * 2;
}

static Status ComputeFourTimes(int x, int* out) {
  HISTO_ASSIGN_OR_RETURN(int doubled, DoubleIfPositive(x));
  HISTO_ASSIGN_OR_RETURN(int quadrupled, DoubleIfPositive(doubled));
  *out = quadrupled;
  return OkStatus();
}

HISTO_TEST(MacrosTest, AssignOrReturn) {
  int out = 0;
  Status s1 = ComputeFourTimes(5, &out);
  EXPECT_TRUE(s1.ok());
  EXPECT_EQ(out, 20);

  Status s2 = ComputeFourTimes(-1, &out);
  EXPECT_FALSE(s2.ok());
  EXPECT_EQ(static_cast<int32_t>(s2.code()), static_cast<int32_t>(StatusCode::kInvalidArgument));
}
