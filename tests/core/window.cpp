#include "test_framework/test_framework.h"
#include <railguard/core/window.h>
#include <railguard/utils/event_sender.h>

#include <iostream>

#ifdef NO_INTERACTIVE
#include <thread>
#endif

TEST
{
    rg::Window window(rg::Extent2D {800, 600}, "Test");

    window.on_resize()->subscribe([](const rg::Extent2D &extent)
                                  { std::cout << "Resized to " << extent.width << "x" << extent.height << std::endl; });

    bool should_quit = false;
    window.on_close()->subscribe([&should_quit](std::nullptr_t _) { should_quit = true; });

#ifdef NO_INTERACTIVE
    // In tests, we want a timeout
    uint64_t timeout = 20;

    // Run sleep in another thread, then send event
    std::thread(
        [&timeout, &should_quit]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            should_quit = true;
        })
        .detach();
#endif

    while (!should_quit)
    {
        window.handle_events();
    }
}