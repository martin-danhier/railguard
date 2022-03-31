# Tests

In this directory, various tests are defined for the railguard library.

They allow:
- to detect errors
- to test previous errors that were fixed to see if they do not reappear
- to give examples of how to use the library

## How to run the tests

Tests are integrated using CTest in the CMake configuration.

The easiest way to run the tests is to use VSCode Test Explorer, which lists them all in a tree automatically.

However, the test explorer doesn't build the tests automatically.

To do that, we need to build the `tests` target, for example using the `Build Tests (Debug)` task (shortcut is `CTRL+SHIFT+B`).

## How to create a test

The tests use a minimalist custom testing framework inspired by Google Test.

To create a test, simply create a new cpp file in one of the test directories,
and start with the following:

```c
#include <test_framework.h>

TEST {
    // Test code
}
```

CMake will automatically find the new test.

Note: the `TEST` macro wraps the `main` function, thus only one test is allow per file.

Inside a test, you can use various macros. They exist in two categories:
- **Assertions**: check if a condition is true. If m_it is false, do not continue the execution.
- **Expectations**: check if a condition is true. If m_it is false, continue the execution.

Typically, use expectations everywhere, except if a false condition could imply segmentation faults later.
For example, if a pointer is followed in the test, assert that the pointer is not null beforehand.

The macros are the following:
- ``ASSERT_TRUE(condition)``: check if a condition is true.
- ``ASSERT_FALSE(condition)``: check if a condition is false.
- ``ASSERT_NULL(pointer)``: check if a pointer is null.
- ``ASSERT_NOT_NULL(pointer)``: check if a pointer is not null.

The equivalent exists for expectations, for example ``EXPECT_TRUE``.

## How to check if we are in a test from the library

``UNIT_TESTS`` is defined when in test mode.