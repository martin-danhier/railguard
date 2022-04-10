#include <railguard/utils/vector.h>

#include "test_framework/test_framework.h"

int call_count[2] = {0, 0};

class Value
{
    int value;

  public:
    explicit Value(int value) : value(value)
    {
    }

    ~Value()
    {
        if (value == 1)
        {
            call_count[0]++;
        }
        else if (value == 2)
        {
            call_count[1]++;
        }

        value = 0;
    }

    [[nodiscard]] int get_value() const
    {
        return value;
    }

    void setValue(int new_value)
    {
        this->value = new_value;
    }
};

TEST
{
    rg::Vector<Value> v(5);

    EXPECT_TRUE(v.is_empty());
    v.push_back(Value(1));
    // The destructor should have been called once
    EXPECT_TRUE(call_count[0] == 1);

    EXPECT_FALSE(v.is_empty());
    v.push_back(Value(2));
    // The destructor should have been called once as well
    EXPECT_TRUE(call_count[1] == 1);

    EXPECT_TRUE(v[0].get_value() == 1);
    EXPECT_TRUE(v[1].get_value() == 2);
    EXPECT_TRUE(v.size() == 2);

    v.pop_back();
    EXPECT_TRUE(v.size() == 1);
    // The destructor should have been called again
    EXPECT_TRUE(call_count[1] == 2);

    v.pop_back();
    EXPECT_TRUE(v.is_empty());
    // The destructor should have been called again
    EXPECT_TRUE(call_count[0] == 2);

    // Test if the resize works

    EXPECT_TRUE(v.capacity() == 5);
    // Fill with 5 elements
    for (int i = 0; i < 5; i++)
    {
        v.push_back(Value(i));
    }
    // The size should be 5
    EXPECT_TRUE(v.size() == 5);
    EXPECT_TRUE(v.capacity());

    // This one should resize the vector by 1
    v.push_back(Value(5));
    EXPECT_TRUE(v.size() == 6);
    EXPECT_TRUE(v.capacity() == 6);

    // This one should resize the vector by 2
    v.push_back(Value(6));
    EXPECT_TRUE(v.size() == 7);
    EXPECT_TRUE(v.capacity() == 8);

    // These 2 by 4
    v.push_back(Value(7));
    v.push_back(Value(8));
    EXPECT_TRUE(v.size() == 9);
    EXPECT_TRUE(v.capacity() == 12);

    // These 4 by 8
    for (int i = 0; i < 4; i++)
    {
        v.push_back(Value(i + 9));
        EXPECT_TRUE(v.size() == i + 10);
    }
    EXPECT_TRUE(v.capacity() == 20);

    // These 8 by 16
    for (int i = 0; i < 8; i++)
    {
        v.push_back(Value(i + 13));
        EXPECT_TRUE(v.size() == i + 14);
    }
    EXPECT_TRUE(v.capacity() == 36);

    // Each value should be i
    for (int i = 0; i < v.size(); i++)
    {
        EXPECT_TRUE(v[i].get_value() == i);
    }

    // And it also works with Iterator
    size_t i = 0;
    for (auto &value : v)
    {
        EXPECT_TRUE(value.get_value() == i++);
    }
    EXPECT_TRUE(i == v.size());

    // And also with const Iterator
    i = 0;
    for (const auto &value : v)
    {
        EXPECT_TRUE(value.get_value() == i++);
    }
    EXPECT_TRUE(i == v.size());
}
