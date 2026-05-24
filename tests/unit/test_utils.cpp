#include <algorithm>
#include <gtest/gtest.h>

#include "core/Utils.h"

using namespace jetson::core;

class UtilsTest : public ::testing::Test
{
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

// String utilities tests
TEST_F(UtilsTest, StringTrim)
{
    EXPECT_EQ(string::trim("  hello  "), "hello");
    EXPECT_EQ(string::trim("\tworld\n"), "world");
    EXPECT_EQ(string::trim("test"), "test");
    EXPECT_EQ(string::trim(""), "");
}

TEST_F(UtilsTest, StringToLower)
{
    EXPECT_EQ(string::to_lower("HELLO"), "hello");
    EXPECT_EQ(string::to_lower("World"), "world");
    EXPECT_EQ(string::to_lower("TeSt"), "test");
}

TEST_F(UtilsTest, StringToUpper)
{
    EXPECT_EQ(string::to_upper("hello"), "HELLO");
    EXPECT_EQ(string::to_upper("World"), "WORLD");
    EXPECT_EQ(string::to_upper("TeSt"), "TEST");
}

TEST_F(UtilsTest, StringSplit)
{
    auto result = string::split("a,b,c", ',');
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST_F(UtilsTest, StringJoin)
{
    std::vector<std::string> v = {"a", "b", "c"};
    EXPECT_EQ(string::join(v, ","), "a,b,c");
    EXPECT_EQ(string::join(v, "-"), "a-b-c");
}

TEST_F(UtilsTest, StringStartsWith)
{
    EXPECT_TRUE(string::starts_with("hello world", "hello"));
    EXPECT_FALSE(string::starts_with("hello world", "world"));
    EXPECT_TRUE(string::starts_with("test", "test"));
}

TEST_F(UtilsTest, StringEndsWith)
{
    EXPECT_TRUE(string::ends_with("hello world", "world"));
    EXPECT_FALSE(string::ends_with("hello world", "hello"));
    EXPECT_TRUE(string::ends_with("test", "test"));
}

TEST_F(UtilsTest, StringReplaceAll)
{
    EXPECT_EQ(string::replace_all("hello world", "o", "a"), "hella warld");
    EXPECT_EQ(string::replace_all("test test", "test", "demo"), "demo demo");
    EXPECT_EQ(string::replace_all("no match", "xyz", "abc"), "no match");
}

// Time utilities tests
TEST_F(UtilsTest, TimeNow)
{
    auto now_us = time::now_us();
    auto now_ms = time::now_ms();
    auto now_s = time::now_s();

    EXPECT_GT(now_us, 0);
    EXPECT_GT(now_ms, 0);
    EXPECT_GT(now_s, 0);

    // Verify relationship between units
    EXPECT_GT(now_us, now_ms * 1000);
    EXPECT_GT(now_ms, now_s * 1000);
}

TEST_F(UtilsTest, TimeFormatTimestamp)
{
    auto formatted = time::format_timestamp(1234567890000LL);
    EXPECT_FALSE(formatted.empty());
    EXPECT_TRUE(formatted.find("T") != std::string::npos);
    EXPECT_TRUE(string::ends_with(formatted, "Z"));
}

// Container utilities tests
TEST_F(UtilsTest, ContainerContains)
{
    std::vector<int> v = {1, 2, 3, 4, 5};
    EXPECT_TRUE(container::contains(v, 3));
    EXPECT_FALSE(container::contains(v, 6));
}

TEST_F(UtilsTest, ContainerRemoveAll)
{
    std::vector<int> v = {1, 2, 3, 2, 4, 2, 5};
    container::remove_all(v, 2);
    EXPECT_EQ(v.size(), 4);
    EXPECT_EQ(std::find(v.begin(), v.end(), 2), v.end());
}

// Scope guard tests
TEST_F(UtilsTest, ScopeGuard)
{
    bool executed = false;
    {
        auto guard = make_scope_guard([&executed]() { executed = true; });
        EXPECT_FALSE(executed);
    }
    EXPECT_TRUE(executed);
}

TEST_F(UtilsTest, ScopeGuardDismiss)
{
    bool executed = false;
    {
        auto guard = make_scope_guard([&executed]() { executed = true; });
        guard.dismiss();
    }
    EXPECT_FALSE(executed);
}
