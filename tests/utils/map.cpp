#include <railguard/utils/map.h>
#include <iostream>

#include <test_framework/test_framework.hpp>

struct Data {
    int a = 0;
    int b = 0;

    bool operator!=(const Data& other) const {
        return a != other.a || b != other.b;
    }
};

TEST {
    rg::Map<Data> map;
    map.set(42, Data{1, 2});
    map.set(43, Data{50, 54});

    auto data = map.get(42);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data.value().a, 1);
    EXPECT_EQ(data.value().b, 2);

    data = map.get(43);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data.value().a, 50);
    EXPECT_EQ(data.value().b, 54);

    data = map.get(44);
    EXPECT_FALSE(data.has_value());

    map.remove(42);
    data = map.get(42);
    EXPECT_FALSE(data.has_value());

    // Set multiple values
    map.set(42, Data{1, 2});
    map.set(43, Data{3, 4});
    map.set(44, Data{5, 6});
    map.set(45, Data{7, 8});
    map.set(46, Data{9, 10});
    map.set(47, Data{11, 12});
    map.set(48, Data{13, 14});

    EXPECT_EQ(map.count(), static_cast<size_t>(7));

    // Iterate over all values
    for (auto &entry: map) {
        entry.value().a += 1;
    }

    EXPECT_EQ(map.count(), static_cast<size_t>(7));

    ASSERT_TRUE(map.get(42).has_value());
    EXPECT_EQ(map.get(42).value().a, 2);
    ASSERT_TRUE(map.get(43).has_value());
    EXPECT_EQ(map.get(43).value().a, 4);
    ASSERT_TRUE(map.get(44).has_value());
    EXPECT_EQ(map.get(44).value().a, 6);
    ASSERT_TRUE(map.get(45).has_value());
    EXPECT_EQ(map.get(45).value().a, 8);
    ASSERT_TRUE(map.get(46).has_value());
    EXPECT_EQ(map.get(46).value().a, 10);
    ASSERT_TRUE(map.get(47).has_value());
    EXPECT_EQ(map.get(47).value().a, 12);
    ASSERT_TRUE(map.get(48).has_value());
    EXPECT_EQ(map.get(48).value().a, 14);

    // Try to update with get
    auto &v = map.get(42).value();
    v.a = 100;

    // We can also use operators if we want a slot for that key to be created if it doesn't exist
    // Note: doing the operation each time will look up the map each time
    // It will be more performant to store the reference at the top
    map[42].a = 200;
    EXPECT_EQ(map[42].a, 200);
    EXPECT_EQ(map[42].b, 2);
    EXPECT_EQ(map.get(42).value().a, 200);
    EXPECT_EQ(map.get(42).value().b, 2);

    map[99].a = 300;
    EXPECT_EQ(map[99].a, 300);
    EXPECT_EQ(map[99].b, 0);
    EXPECT_EQ(map.get(99).value().a, 300);
    EXPECT_EQ(map.get(99).value().b, 0);

    // We can also use the operator in a const way
    const auto &foo = map[42];
    EXPECT_EQ(foo.a, 200);
    EXPECT_EQ(foo.b, 2);




}