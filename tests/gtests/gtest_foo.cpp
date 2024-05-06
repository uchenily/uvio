#include "gtest/gtest.h"

TEST(TestSuiteName, TestName) {
    EXPECT_EQ(2, [](int a, int b) {
        return a + b;
    }(1, 1));
}
