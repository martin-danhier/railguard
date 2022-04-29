#include <railguard/utils/array.h>

#include <test_framework/test_framework.hpp>

TEST
{
    // Array of array
    rg::Array<rg::Array<int>> aa {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    EXPECT_EQ(aa[0][0], 1);
    EXPECT_EQ(aa[0][1], 2);
    EXPECT_EQ(aa[0][2], 3);

    EXPECT_EQ(aa[1][0], 4);
    EXPECT_EQ(aa[1][1], 5);
    EXPECT_EQ(aa[1][2], 6);

    EXPECT_EQ(aa[2][0], 7);
    EXPECT_EQ(aa[2][1], 8);
    EXPECT_EQ(aa[2][2], 9);
}