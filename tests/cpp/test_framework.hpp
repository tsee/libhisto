// Minimal, dependency-free Google-style test framework for libhisto C++ tests.
// Exception-free, compliant with -fno-exceptions, -Wall, -Wextra, -Werror.

#ifndef LIBHISTO_TEST_FRAMEWORK_HPP
#define LIBHISTO_TEST_FRAMEWORK_HPP

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace histo_test {

struct TestCase {
  std::string name;
  void (*func)();
};

inline std::vector<TestCase>& GetTestRegistry() {
  static std::vector<TestCase> registry;
  return registry;
}

inline bool RegisterTest(const std::string& name, void (*func)()) {
  GetTestRegistry().push_back({name, func});
  return true;
}

inline int& GetFailureCount() {
  static int failures = 0;
  return failures;
}

inline int RunAllTests() {
  int total = static_cast<int>(GetTestRegistry().size());
  int passed = 0;
  std::cout << "[==========] Running " << total << " tests from test suite.\n";

  for (const auto& test : GetTestRegistry()) {
    std::cout << "[ RUN      ] " << test.name << "\n";
    int old_failures = GetFailureCount();
    test.func();
    if (GetFailureCount() == old_failures) {
      std::cout << "[       OK ] " << test.name << "\n";
      passed++;
    } else {
      std::cout << "[  FAILED  ] " << test.name << "\n";
    }
  }

  std::cout << "[==========] " << total << " tests ran. (" << passed << " passed, "
            << (total - passed) << " failed)\n";
  return (total == passed) ? 0 : 1;
}

}  // namespace histo_test

#define HISTO_TEST(suite, name)                                                \
  void suite##_##name();                                                       \
  namespace {                                                                  \
  struct suite##_##name##_Reg {                                                \
    suite##_##name##_Reg() {                                                   \
      ::histo_test::RegisterTest(#suite "." #name, suite##_##name);            \
    }                                                                          \
  };                                                                           \
  static const suite##_##name##_Reg suite##_##name##_instance;                 \
  }                                                                            \
  void suite##_##name()

#define EXPECT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "  [FAILED] " << __FILE__ << ":" << __LINE__               \
                << " - Expected true: " #cond "\n";                            \
      ::histo_test::GetFailureCount()++;                                       \
    }                                                                          \
  } while (0)

#define EXPECT_FALSE(cond)                                                     \
  do {                                                                         \
    if (cond) {                                                                \
      std::cerr << "  [FAILED] " << __FILE__ << ":" << __LINE__               \
                << " - Expected false: " #cond "\n";                           \
      ::histo_test::GetFailureCount()++;                                       \
    }                                                                          \
  } while (0)

#define EXPECT_EQ(val1, val2)                                                  \
  do {                                                                         \
    if ((val1) != (val2)) {                                                    \
      std::cerr << "  [FAILED] " << __FILE__ << ":" << __LINE__               \
                << " - Expected equality of (" #val1 ") and (" #val2 ")\n"     \
                << "    Actual: " << (val1) << " vs " << (val2) << "\n";       \
      ::histo_test::GetFailureCount()++;                                       \
    }                                                                          \
  } while (0)

#define EXPECT_NEAR(val1, val2, eps)                                           \
  do {                                                                         \
    double diff = std::fabs(static_cast<double>(val1) - static_cast<double>(val2)); \
    if (diff > (eps)) {                                                        \
      std::cerr << "  [FAILED] " << __FILE__ << ":" << __LINE__               \
                << " - Expected (" #val1 ") near (" #val2 ") within " #eps "\n"\
                << "    Actual: " << (val1) << " vs " << (val2)                \
                << " (diff = " << diff << ")\n";                               \
      ::histo_test::GetFailureCount()++;                                       \
    }                                                                          \
  } while (0)

#define EXPECT_STATUS_OK(status_expr)                                          \
  do {                                                                         \
    auto _st = (status_expr);                                                  \
    if (!_st.ok()) {                                                           \
      std::cerr << "  [FAILED] " << __FILE__ << ":" << __LINE__               \
                << " - Expected OK status for: " #status_expr "\n"             \
                << "    Error message: " << _st.message() << "\n";             \
      ::histo_test::GetFailureCount()++;                                       \
    }                                                                          \
  } while (0)

#endif  // LIBHISTO_TEST_FRAMEWORK_HPP
