#include <railguard/utils/vector.h>

#include "test_framework/test_framework.h"

TEST
{
    // Create root vector
    rg::Vector<rg::Vector<int>> v(5);

    // Create sub vector with && rvalue
    v.push_back(rg::Vector<int>(3));

    // We should be able to push m_data into it
    v[0].push_back(1);
    v[0].push_back(3);

    // And retrieve it
    EXPECT_TRUE(v[0][0] == 1);
    EXPECT_TRUE(v[0][1] == 3);

    // We should be able to push another vector
    v.push_back(rg::Vector<int>(4));

    // It should be able to resize
    for (int i = 0; i < 30; i++)
    {
        v[1].push_back(i);
        EXPECT_TRUE(v[1][i] == i);
    }

    // That's also true for v
    for (int i = 0; i < 10; i++)
    {
        v.push_back(rg::Vector<int>());
    }

    // Data should still be in the first ones
    for (int i = 0; i < 30; i++)
    {
        EXPECT_TRUE(v[1][i] == i);
    }

    // We should be able to pop some of them
    for (int i = 0; i < 5; i++)
    {
        v.pop_back();
    }

    // We should be able to copy the second vector in the first slot, and destructors will be called correctly
    EXPECT_TRUE(v.copy(1, 0));

    // Data should be in both
    for (int i = 0; i < 30; i++)
    {
        EXPECT_TRUE(v[0][i] == i);
        EXPECT_TRUE(v[1][i] == i);
    }

    // Though the same, the first vector should be a copy of the second
    // Thus, we should be able to update the second independently of the first
    // We apply some modifications
    v[1][0]  = 42;
    v[1][10] = 27;
    v[1][18] = 7;

    // Data should still be the same as before in the first vector
    for (int i = 0; i < 30; i++)
    {
        EXPECT_TRUE(v[0][i] == i);
    }

    // Clear should work
    v.clear();

    EXPECT_TRUE(v.is_empty());
}