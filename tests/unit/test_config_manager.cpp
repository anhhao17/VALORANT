#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>

#include "core/ConfigManager.h"

using namespace jetson::core;

class ConfigManagerTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create a temporary test config file
        test_config_path_ = "test_config.yaml";
        std::ofstream config_file(test_config_path_);
        config_file << R"(
server:
  address: "127.0.0.1"
  port: 8080
  threads: 4

auth:
  jwt_secret: "test_secret"
  token_expiry_seconds: 3600

logging:
  level: "debug"

features:
  streaming: true
  llm: false

sensors:
  - cpu_temp
  - gpu_temp
  - ram_usage
)";
        config_file.close();
    }

    void TearDown() override { std::remove(test_config_path_.c_str()); }

    std::string test_config_path_;
};

TEST_F(ConfigManagerTest, LoadConfig)
{
    ConfigManager config;
    EXPECT_TRUE(config.load(test_config_path_));
    EXPECT_EQ(config.config_path(), test_config_path_);
}

TEST_F(ConfigManagerTest, GetString)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    EXPECT_EQ(config.get_string("server.address"), "127.0.0.1");
    EXPECT_EQ(config.get_string("server.port"), "8080");
    EXPECT_EQ(config.get_string("nonexistent.key", "default"), "default");
}

TEST_F(ConfigManagerTest, GetInt)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    EXPECT_EQ(config.get_int("server.port"), 8080);
    EXPECT_EQ(config.get_int("server.threads"), 4);
    EXPECT_EQ(config.get_int("nonexistent.key", 42), 42);
}

TEST_F(ConfigManagerTest, GetBool)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    EXPECT_TRUE(config.get_bool("features.streaming"));
    EXPECT_FALSE(config.get_bool("features.llm"));
    EXPECT_TRUE(config.get_bool("nonexistent.key", true));
}

TEST_F(ConfigManagerTest, GetStringList)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    auto sensors = config.get_string_list("sensors");
    EXPECT_EQ(sensors.size(), 3);
    EXPECT_EQ(sensors[0], "cpu_temp");
    EXPECT_EQ(sensors[1], "gpu_temp");
    EXPECT_EQ(sensors[2], "ram_usage");
}

TEST_F(ConfigManagerTest, HasKey)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    EXPECT_TRUE(config.has_key("server.address"));
    EXPECT_TRUE(config.has_key("auth.jwt_secret"));
    EXPECT_FALSE(config.has_key("nonexistent.key"));
}

TEST_F(ConfigManagerTest, EnvVarSubstitution)
{
    // Set environment variable
    setenv("TEST_VAR", "test_value", 1);

    std::ofstream config_file("test_env_config.yaml");
    config_file << "test_key: \"${TEST_VAR}\"\n";
    config_file.close();

    ConfigManager config;
    ASSERT_TRUE(config.load("test_env_config.yaml"));
    EXPECT_EQ(config.get_string("test_key"), "test_value");

    std::remove("test_env_config.yaml");
    unsetenv("TEST_VAR");
}

TEST_F(ConfigManagerTest, Reload)
{
    ConfigManager config;
    ASSERT_TRUE(config.load(test_config_path_));

    // Modify the file
    {
        std::ofstream config_file(test_config_path_, std::ios::app);
        config_file << "new_key: new_value\n";
    }

    EXPECT_TRUE(config.reload());
    EXPECT_EQ(config.get_string("new_key"), "new_value");
}