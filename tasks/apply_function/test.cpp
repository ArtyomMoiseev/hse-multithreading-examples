#include "apply_function.h"

#include <string>

#include <gtest/gtest.h>

TEST(ApplyFunction, SingleThread) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    ApplyFunction<int>(data, [](int& x) { x *= 2; }, 1);
    EXPECT_EQ(data, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(ApplyFunction, StringTransform) {
    std::vector<std::string> data = {"hello", "world", "hse"};
    ApplyFunction<std::string>(data, [](std::string& s) { s += "!"; }, 2);
    EXPECT_EQ(data, (std::vector<std::string>{"hello!", "world!", "hse!"}));
}

TEST(ApplyFunction, MultiThread) {
    std::vector<int> data(1000);
    for (int i = 0; i < 1000; ++i) {
        data[i] = i;
    }
    ApplyFunction<int>(data, [](int& x) { x += 10; }, 4);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(data[i], i + 10);
    }
}
