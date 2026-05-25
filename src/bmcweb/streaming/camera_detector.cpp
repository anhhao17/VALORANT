#include "camera_detector.hpp"
#include "../logging.hpp"
#include <filesystem>

namespace embed::bmcweb::streaming
{

std::vector<StreamConfig> CameraDetector::detectCameras()
{
    return detectCameras(4); // Default: check first 4 video devices
}

std::vector<StreamConfig> CameraDetector::detectCameras(int maxDevices)
{
    std::vector<StreamConfig> cameras;
    
    // Detect video devices
    for (int i = 0; i < maxDevices; i++)
    {
        std::string device = "/dev/video" + std::to_string(i);
        if (std::filesystem::exists(device))
        {
            cameras.push_back(createCameraConfig(device, i));
        }
    }
    
    LOG_INFO("Auto-detected {} cameras", cameras.size());
    return cameras;
}

bool CameraDetector::cameraExists(const std::string& devicePath)
{
    return std::filesystem::exists(devicePath);
}

StreamConfig CameraDetector::createCameraConfig(const std::string& device, int index)
{
    StreamConfig config;
    config.id = "camera_" + std::to_string(index);
    config.name = "Camera " + std::to_string(index);
    config.type = StreamSourceType::CAMERA_DEVICE;
    config.protocol = StreamProtocol::MJPEG; // Default protocol
    config.sourcePath = device;
    config.enabled = true;
    config.loop = false;
    config.quality = 80;
    config.bufferSize = 1048576; // 1MB default
    config.segmentDuration = 10; // 10 seconds default
    config.port = 8554 + index;
    
    LOG_INFO("Auto-detected camera: {} at {}", config.id, device);
    return config;
}

} // namespace embed::bmcweb::streaming