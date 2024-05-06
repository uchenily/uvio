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
    void SetUp() override {
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
