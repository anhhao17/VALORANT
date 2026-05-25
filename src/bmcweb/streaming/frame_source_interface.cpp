#include "frame_source_interface.hpp"
#include "camera_capture.hpp"
#include "file_capture.hpp"
#include "test_pattern_capture.hpp"
#include "../logging.hpp"

namespace embed::bmcweb::streaming
{

std::unique_ptr<IFrameSource> FrameSourceFactory::createFrameSource(FrameSourceType type)
{
    switch (type)
    {
        case FrameSourceType::CAMERA_DEVICE:
            LOG_INFO("Creating CameraCapture instance");
            return std::make_unique<CameraCapture>();
            
        case FrameSourceType::VIDEO_FILE:
            LOG_INFO("Creating FileCapture instance");
            return std::make_unique<FileCapture>();
            
        case FrameSourceType::TEST_PATTERN:
            LOG_INFO("Creating TestPatternCapture instance");
            return std::make_unique<TestPatternCapture>();
            
        case FrameSourceType::NETWORK_STREAM:
            LOG_INFO("NetworkStreamCapture not yet implemented");
            return nullptr; // TODO: Implement NetworkStreamCapture
            
        case FrameSourceType::IMAGE_SEQUENCE:
            LOG_INFO("ImageSequenceCapture not yet implemented");
            return nullptr; // TODO: Implement ImageSequenceCapture
            
        default:
            LOG_ERROR("Unsupported frame source type: {}", static_cast<int>(type));
            return nullptr;
    }
}

std::vector<FrameSourceType> FrameSourceFactory::getSupportedSourceTypes()
{
    return {
        FrameSourceType::CAMERA_DEVICE,
        FrameSourceType::VIDEO_FILE,
        FrameSourceType::TEST_PATTERN,
        FrameSourceType::NETWORK_STREAM,
        FrameSourceType::IMAGE_SEQUENCE
    };
}

bool FrameSourceFactory::isSourceTypeSupported(FrameSourceType type)
{
    auto supported = getSupportedSourceTypes();
    for (auto supportedType : supported)
    {
        if (supportedType == type)
        {
            return true;
        }
    }
    return false;
}

} // namespace embed::bmcweb::streaming