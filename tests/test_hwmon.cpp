#include <gtest/gtest.h>

#include "LinuxSysfsMonitor.h"
#include "SensorData.h"
#include "HwMonFactory.h"

using namespace jetson::hwmon;

TEST(LinuxSysfsMonitor, Initialization)
{
    LinuxSysfsMonitor monitor;
    EXPECT_TRUE(monitor.initialize());
    EXPECT_EQ(monitor.name(), "LinuxSysfsMonitor");
}

TEST(LinuxSysfsMonitor, ReadSensors)
{
    LinuxSysfsMonitor monitor;
    ASSERT_TRUE(monitor.initialize());

    auto sensors = monitor.read_sensors();
    EXPECT_FALSE(sensors.empty());

    // Check that we have at least some basic sensors
    bool has_memory = false;
    bool has_disk = false;

    for (const auto& sensor : sensors)
    {
        if (sensor.type == SensorType::MEMORY_USAGE)
        {
            has_memory = true;
            EXPECT_GE(sensor.value, 0.0);
            EXPECT_LE(sensor.value, 100.0);
            EXPECT_EQ(sensor.unit, "%");
        }
        if (sensor.type == SensorType::DISK_USAGE)
        {
            has_disk = true;
            EXPECT_GE(sensor.value, 0.0);
            EXPECT_LE(sensor.value, 100.0);
            EXPECT_EQ(sensor.unit, "%");
        }
    }

    EXPECT_TRUE(has_memory);
    EXPECT_TRUE(has_disk);
}

TEST(LinuxSysfsMonitor, GetSnapshot)
{
    LinuxSysfsMonitor monitor;
    ASSERT_TRUE(monitor.initialize());

    auto snapshot = monitor.get_snapshot();
    EXPECT_FALSE(snapshot.sensors.empty());
    EXPECT_FALSE(snapshot.platform.platform.empty());
}

TEST(LinuxSysfsMonitor, PlatformInfo)
{
    LinuxSysfsMonitor monitor;
    ASSERT_TRUE(monitor.initialize());

    auto platform = monitor.get_platform_info();
    EXPECT_EQ(platform.platform, "linux");
    EXPECT_FALSE(platform.cpu_arch.empty());
}

TEST(SensorData, SensorTypeConversion)
{
    EXPECT_EQ(sensor_type_to_string(SensorType::CPU_TEMP), "cpu_temp");
    EXPECT_EQ(sensor_type_to_string(SensorType::GPU_TEMP), "gpu_temp");
    EXPECT_EQ(sensor_type_to_string(SensorType::MEMORY_USAGE), "memory_usage");

    EXPECT_EQ(string_to_sensor_type("cpu_temp"), SensorType::CPU_TEMP);
    EXPECT_EQ(string_to_sensor_type("gpu_temp"), SensorType::GPU_TEMP);
    EXPECT_EQ(string_to_sensor_type("memory_usage"), SensorType::MEMORY_USAGE);
}

TEST(SensorData, HardwareSnapshot)
{
    HardwareSnapshot snapshot;

    SensorReading temp1("cpu0", SensorType::CPU_TEMP, 45.5, "°C");
    SensorReading temp2("cpu1", SensorType::CPU_TEMP, 47.2, "°C");
    SensorReading mem("memory", SensorType::MEMORY_USAGE, 65.0, "%");

    snapshot.add_sensor(temp1);
    snapshot.add_sensor(temp2);
    snapshot.add_sensor(mem);

    EXPECT_EQ(snapshot.sensors.size(), 3);

    const SensorReading* found = snapshot.get_sensor("cpu0");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 45.5);

    auto cpu_temps = snapshot.get_sensors_by_type(SensorType::CPU_TEMP);
    EXPECT_EQ(cpu_temps.size(), 2);

    std::string json = snapshot.to_json();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("cpu0"), std::string::npos);
}

TEST(HwMonFactory, PlatformDetection)
{
    std::string platform = HwMonFactory::detect_platform();
    EXPECT_FALSE(platform.empty());

    // On current system, should be "linux" since we're not on Jetson/RPi/BBB
    EXPECT_TRUE(platform == "linux" || platform == "jetson" || platform == "rpi" ||
                platform == "bbb");
}

TEST(HwMonFactory, CreateMonitor)
{
    auto monitor = HwMonFactory::create();
    ASSERT_NE(monitor, nullptr);
    EXPECT_TRUE(monitor->initialize());

    auto sensors = monitor->read_sensors();
    EXPECT_FALSE(sensors.empty());
}
