#include <railguard/utils/io.h>

#include <string>
#include <test_framework/test_framework.hpp>

TEST
{
    std::string EXPECTED("This is a file containing test text.");

    // Test reading a file
    size_t      length   = 0;
    char       *contents = nullptr;
    ASSERT_NO_THROWS(contents = static_cast<char *>(rg::load_binary_file("resources/test.txt", &length)));
    std::string s        = std::string(contents, length);
    EXPECT_EQ(s, EXPECTED);
    EXPECT_TRUE(length == EXPECTED.size());

    // Free memory
    delete[] contents;
    contents = nullptr;

    // A non-existing file throws an exception
    EXPECT_THROWS(rg::load_binary_file("resources/non-existing.txt", &length));
}