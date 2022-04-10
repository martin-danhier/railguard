#include <test_framework/test_framework.hpp>

#define PRETTY_PRINT_MAT4
#define IMPLEMENTATION_MAT4
#include <railguard/utils/geometry/mat4.h>

TEST {
    rg::Mat4 identity = rg::Mat4::identity();
    // clang-format off
    rg::Mat4 m2(
        1,   0,  0,   0,
        2,   8,  3,   0,
        9,   12, 2.6, -1,
        767, -1, 1,   22
    );
    // clang-format on

    // Matrix multiplication
    auto res = identity * m2;
    EXPECT_EQ(res, m2);
    EXPECT_NEQ(res, identity);

    auto res2 = m2 * identity;
    EXPECT_EQ(res2, m2);
    EXPECT_NEQ(res2, identity);

    auto res3 = m2 * m2;
    // clang-format off
    constexpr auto expected = rg::Mat4(
        1,      0,     0,    0,
        45,     100,   31.8, -3,
        -710.6, 128.2, 41.76, -24.6,
        17648,  -18,   21.6,  483
    );
    // clang-format on

    EXPECT_EQ(res3, expected);
    EXPECT_NEQ(res3, m2);



}