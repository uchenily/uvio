#include "gtest/gtest.h"

// ===== TEST =====

TEST(TestSuiteName, TestName) {
    EXPECT_EQ(2, [](int a, int b) {
        return a + b;
    }(1, 1));
}

// ===== TEST_F =====

class TestFixtureName : public testing::Test {
protected:
    auto SetUp() -> void override {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
    }
    std::vector<int> vec;
};

TEST_F(TestFixtureName, TestName) {
    vec.push_back(4);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec.back(), 4);
}

TEST_F(TestFixtureName, TestName2) {
    EXPECT_EQ(vec.size(), 3);
}

// ===== TEST_P ===== (P: parameterized)

class TestSuiteParameterized : public testing::TestWithParam<int> {};

auto IsPrime(int num) -> bool {
    if (num <= 1) {
        return false;
    }
    for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) {
            return false;
        }
    }
    return true;
}

INSTANTIATE_TEST_CASE_P(TestSuiteParameterizedName,
                        TestSuiteParameterized,
                        testing::Values(3, 5, 11, 23, 17));

TEST_P(TestSuiteParameterized, TestName) {
    int n = GetParam();
    EXPECT_TRUE(IsPrime(n));
}
