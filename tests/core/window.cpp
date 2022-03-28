#include <railguard/core/window.h>
#include <railguard/utils/vector.h>

#include "test_framework/test_framework.h"

TEST
{
    rg::Window window(rg::Extend2D{800, 600}, "Test");
}