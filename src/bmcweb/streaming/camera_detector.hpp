#pragma once

#include "stream_types.hpp"
#include <vector>
#include <string>

namespace embed::bmcweb::streaming
{

/**
 * @brief Camera Detector - handles camera auto-detection
 * 
 * Automatically detects available camera devices and creates stream configurations.
 */
class CameraDetector
{
public:
    CameraDetector() = default;
    ~CameraDetector() = default;
    
    // Camera detection
    std::vector<StreamConfig> detectCameras();
    std::vector<StreamConfig> detectCameras(int maxDevices);
    
    // Check if specific camera device exists
    bool cameraExists(const std::string& devicePath);
    
private:
    // Create stream config from camera device
    StreamConfig createCameraConfig(const std::string& device, int index);
};

} // namespace embed::bmcweb::streaming