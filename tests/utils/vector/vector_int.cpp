#include <railguard/utils/vector.h>

#include <test_framework/test_framework.hpp>

TEST {
    rg::Vector<int> v(3);

    EXPECT_TRUE(v.is_empty());

    v.push_back(1);

    EXPECT_FALSE(v.is_empty());
    EXPECT_EQ(v.size(), static_cast<size_t>(1));
    EXPECT_EQ(v[0], 1);

    v.push_back(2);
    v.push_back(3);

    EXPECT_EQ(v.size(), static_cast<size_t>(3));
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);

    v.pop_back();

    // Try to create one in the heap
    auto *v2 = new rg::Vector<int>(3);
    v2->push_back(1);
    v2->push_back(2);
    v2->push_back(3);
    EXPECT_TRUE(v2->operator[](0) == 1);
    EXPECT_TRUE(v2->operator[](1) == 2);
    EXPECT_TRUE(v2->operator[](2) == 3);

    rg::ArrayLike<int> *v3 = v2;
    EXPECT_TRUE(v3->operator[](0) == 1);
    EXPECT_TRUE(v3->operator[](1) == 2);
    EXPECT_TRUE(v3->operator[](2) == 3);

    delete v2;

}