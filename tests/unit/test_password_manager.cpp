#include <auth/PasswordManager.h>
#include <gtest/gtest.h>

using namespace jetson::auth;

TEST(PasswordManager, HashPassword)
{
    std::string password = "test_password_123";
    std::string hash = PasswordManager::hash_password(password);

    // Hash should not be empty
    ASSERT_FALSE(hash.empty());

    // Hash should follow the format: $pbkdf2-sha256$iterations$salt$hash
    ASSERT_TRUE(hash.find("$pbkdf2-sha256$") == 0);

    // Hash should be different from password
    ASSERT_NE(hash, password);
}

TEST(PasswordManager, VerifyPassword)
{
    std::string password = "test_password_123";
    std::string hash = PasswordManager::hash_password(password);

    // Correct password should verify
    ASSERT_TRUE(PasswordManager::verify_password(password, hash));

    // Wrong password should not verify
    ASSERT_FALSE(PasswordManager::verify_password("wrong_password", hash));
}

TEST(PasswordManager, DifferentPasswordsDifferentHashes)
{
    std::string password1 = "password1";
    std::string password2 = "password2";

    std::string hash1 = PasswordManager::hash_password(password1);
    std::string hash2 = PasswordManager::hash_password(password2);

    // Different passwords should produce different hashes
    ASSERT_NE(hash1, hash2);
}

TEST(PasswordManager, SamePasswordDifferentHashes)
{
    std::string password = "same_password";

    std::string hash1 = PasswordManager::hash_password(password);
    std::string hash2 = PasswordManager::hash_password(password);

    // Same password should produce different hashes (due to random salt)
    ASSERT_NE(hash1, hash2);

    // But both should verify correctly
    ASSERT_TRUE(PasswordManager::verify_password(password, hash1));
    ASSERT_TRUE(PasswordManager::verify_password(password, hash2));
}

TEST(PasswordManager, EmptyPassword)
{
    std::string password = "";
    std::string hash = PasswordManager::hash_password(password);

    // Empty password should still hash
    ASSERT_FALSE(hash.empty());
    ASSERT_TRUE(PasswordManager::verify_password(password, hash));
}

TEST(PasswordManager, LongPassword)
{
    std::string password(1000, 'a');  // 1000 character password
    std::string hash = PasswordManager::hash_password(password);

    // Long password should hash
    ASSERT_FALSE(hash.empty());
    ASSERT_TRUE(PasswordManager::verify_password(password, hash));
}

TEST(PasswordManager, SpecialCharacters)
{
    std::string password = "p@$$w0rd!#$%^&*()_+-=[]{}|;':,.<>?/~`";
    std::string hash = PasswordManager::hash_password(password);

    // Special characters should work
    ASSERT_FALSE(hash.empty());
    ASSERT_TRUE(PasswordManager::verify_password(password, hash));
}
