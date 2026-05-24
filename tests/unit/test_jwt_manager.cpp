#include <gtest/gtest.h>

#include "auth/JwtManager.h"

using jetson::auth::JwtManager;

class JwtManagerTest : public ::testing::Test
{
   protected:
    JwtManager jwt;

    void SetUp() override { jwt.configure("test-secret-key", 3600, 86400); }
};

TEST_F(JwtManagerTest, IssueAndVerifyAccess)
{
    auto token = jwt.issue_access("alice");
    auto claims = jwt.verify_access(token);
    EXPECT_EQ(claims.subject, "alice");
    EXPECT_EQ(claims.type, "access");
}

TEST_F(JwtManagerTest, IssueAndVerifyRefresh)
{
    auto token = jwt.issue_refresh("bob");
    auto claims = jwt.verify_refresh(token);
    EXPECT_EQ(claims.subject, "bob");
    EXPECT_EQ(claims.type, "refresh");
}

TEST_F(JwtManagerTest, AccessTokenCannotBeUsedAsRefresh)
{
    auto token = jwt.issue_access("alice");
    EXPECT_THROW(jwt.verify_refresh(token), std::runtime_error);
}

TEST_F(JwtManagerTest, RefreshTokenCannotBeUsedAsAccess)
{
    auto token = jwt.issue_refresh("alice");
    EXPECT_THROW(jwt.verify_access(token), std::runtime_error);
}

TEST_F(JwtManagerTest, TamperedSignatureFails)
{
    auto token = jwt.issue_access("alice");
    // Corrupt the last character of the signature
    auto bad_token = token;
    bad_token.back() = (bad_token.back() == 'A') ? 'B' : 'A';
    EXPECT_THROW(jwt.verify_access(bad_token), std::runtime_error);
}

TEST_F(JwtManagerTest, WrongSecretFails)
{
    auto token = jwt.issue_access("alice");
    JwtManager other;
    other.configure("different-secret", 3600, 86400);
    EXPECT_THROW(other.verify_access(token), std::runtime_error);
}

TEST_F(JwtManagerTest, ExpiredTokenFails)
{
    JwtManager short_jwt;
    short_jwt.configure("secret", 0, 0);  // 0-second expiry → immediately expired
    auto token = short_jwt.issue_access("alice");
    EXPECT_THROW(short_jwt.verify_access(token), std::runtime_error);
}

TEST_F(JwtManagerTest, MalformedTokenFails)
{
    EXPECT_THROW(jwt.verify_access("not.a.valid.jwt.at.all"), std::runtime_error);
    EXPECT_THROW(jwt.verify_access("onlytwoparts.here"), std::runtime_error);
    EXPECT_THROW(jwt.verify_access(""), std::runtime_error);
}

TEST_F(JwtManagerTest, AccessExpiryReflectsConfig)
{
    JwtManager custom;
    custom.configure("s", 7200, 86400);
    EXPECT_EQ(custom.access_expiry(), 7200);
}
