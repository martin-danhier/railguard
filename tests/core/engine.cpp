#include "test_framework/test_framework.hpp"

#include <railguard/core/engine.h>

TEST
{
    rg::Engine engine;
    EXPECT_NO_THROWS(engine.run_main_loop());
}