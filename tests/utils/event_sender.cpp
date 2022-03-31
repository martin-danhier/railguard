#include <railguard/utils/event_sender.h>

#include <test_framework/test_framework.hpp>

struct EventData {
    int value;
};

TEST {
    // Create sender
    rg::EventSender<EventData> sender;

    // Create event data
    EventData event_data{1524};

    // Subscribe to event
    bool called = false;
    auto id = sender.subscribe(
            [&](const EventData &data)
            {
            EXPECT_EQ(data.value, event_data.value);
            // Not called several times
            EXPECT_FALSE(called);
            called = true;
            });

    EXPECT_FALSE(called);

    // Send event
    sender.send(event_data);

    EXPECT_TRUE(called);

    // Unregister
    sender.unsubscribe(id);

    // Send event
    sender.send(event_data);

    // Since the EXPECT in the callback checks for the called variable, if it works, if wasn't called again

    // Register more listeners
    int call_count_1 = 0;
    int call_count_2 = 0;
    auto id_1 = sender.subscribe(
                [&](const EventData &data)
                {
            EXPECT_EQ(data.value, event_data.value);
            call_count_1++;
                });
    auto id_2 = sender.subscribe(
        [&](const EventData &data)
        {
            EXPECT_EQ(data.value, event_data.value);
            call_count_2++;
        });
    EXPECT_EQ(id_1, static_cast<size_t>(2));
    EXPECT_EQ(id_2, static_cast<size_t>(3));
    EXPECT_EQ(call_count_1, 0);
    EXPECT_EQ(call_count_2, 0);

    // Send event
    sender.send(event_data);

    EXPECT_EQ(call_count_1, 1);
    EXPECT_EQ(call_count_2, 1);

    // Unregister one of them
    sender.unsubscribe(id_1);

    // Send event
    sender.send(event_data);

    EXPECT_EQ(call_count_1, 1);
    EXPECT_EQ(call_count_2, 2);


}