/**
 * Extension of the testing framework with more types of expectations, for C++.
 */
#pragma once

#include <functional>
#include <string>
#include <memory>
#include "test_framework.h"

// Macros

// clang-format off
// keep the formatting as below: we don't want line breaks
// It needs to be in a single line so that the __LINE__ macro is accurate

#define EXPECT_THROWS(fn) do { if (!tf_assert_throws(___context___, __LINE__, __FILE__, [&](){fn;}, true)) return; } while (0)
#define EXPECT_NO_THROWS(fn) do { if (!tf_assert_no_throws(___context___, __LINE__, __FILE__, [&](){fn;}, true)) return; } while (0)
#define EXPECT_EQ(actual, expected) do { if (!tf_assert_equal(___context___, __LINE__, __FILE__, (actual), (expected), true)) return; } while (0)
#define ASSERT_THROWS(fn) do { if (!tf_assert_throws(___context___, __LINE__, __FILE__, [&](){fn;}, false)) return; } while (0)
#define ASSERT_NO_THROWS(fn) do { if (!tf_assert_no_throws(___context___, __LINE__, __FILE__, [&](){fn;}, false)) return; } while (0)
#define ASSERT_EQ(actual, expected) do { if (!tf_assert_equal(___context___, __LINE__, __FILE__, (actual), (expected), false)) return; } while (0)

// clang-format on

// Functions

// lambda type
using tf_callback = std::function<void()>;

bool tf_assert_throws(tf_context *context, size_t line_number, const char *file, const tf_callback& fn, bool recoverable);
bool tf_assert_no_throws(tf_context *context, size_t line_number, const char *file, const tf_callback& fn, bool recoverable);

template<typename T>
bool tf_assert_equal(tf_context *context, size_t line_number, const char *file, const T &actual, const T &expected, bool recoverable)
{
    if (actual != expected)
    {
        std::string s = recoverable ? "Condition" : "Assertion";
        s += " failed. Expected: ";
        s += std::to_string(expected);
        s += ", got: ";
        s += std::to_string(actual);
        s += ".";
        s += recoverable ? "" : " Unable to continue execution.";


        return tf_assert_common(context, line_number, file, false, tf_dynamic_msg(s.c_str()), recoverable);
    }
    return true;
}
