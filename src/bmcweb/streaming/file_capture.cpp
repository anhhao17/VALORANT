#include "file_capture.hpp"
#include "../logging.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstring>

#ifdef JETSON_ENABLE_STREAMING
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace embed::bmcweb::streaming
{

FileCapture::FileCapture()
    : capturing_(false)
    , looping_(true)
    , shouldStop_(false)
    , fileDuration_(0)
    , currentPosition_(0)
#ifdef JETSON_ENABLE_STREAMING
    , formatContext_(nullptr)
    , codecContext_(nullptr)
    , videoStreamIndex_(-1)
    , swsContext_(nullptr)
    , frame_(nullptr)
    , rgbFrame_(nullptr)
    , ffmpegInitialized_(false)
#endif
    , framesGenerated_(0)
    , bytesGenerated_(0)
    , startTime_(0)
{
}

FileCapture::~FileCapture()
{
    cleanup();
}

bool FileCapture::initialize(const FrameSourceConfig& config)
{
    LOG_INFO("FileCapture::initialize called for: {}", config.sourcePath);
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Set defaults if not specified
    if (config_.width == 0) config_.width = 640;
    if (config_.height == 0) config_.height = 480;
    if (config_.frameRate == 0) config_.frameRate = 30;
    if (config_.pixelFormat.empty()) config_.pixelFormat = "RGB24";
    looping_ = config.loop;
    
    capturing_ = false;
    currentPosition_ = 0;
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    
    LOG_INFO("Loading video file: {}", config_.sourcePath);
    // Load video file
    if (!loadVideoFile())
    {
        LOG_ERROR("Failed to load video file: {}", config_.sourcePath);
        return false;
    }
    
    LOG_INFO("FileCapture initialized: {} ({}x{} @ {}fps, duration: {}ms, loop: {})", 
             config_.sourcePath, config_.width, config_.height, config_.frameRate, 
             fileDuration_, looping_);
    return true;
}

bool FileCapture::startCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (capturing_.load())
    {
        LOG_WARN("FileCapture already capturing");
        return false;
    }
    
    capturing_ = true;
    shouldStop_ = false;
    startTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Start capture thread
    captureThread_ = std::thread(&FileCapture::captureLoop, this);
    
    LOG_INFO("FileCapture started");
    return true;
}

bool FileCapture::stopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!capturing_.load())
    {
        LOG_WARN("FileCapture not capturing");
        return false;
    }
    
    capturing_ = false;
    shouldStop_ = true;
    
    if (captureThread_.joinable())
    {
        captureThread_.join();
    }
    
    LOG_INFO("FileCapture stopped");
    return true;
}

bool FileCapture::isCapturing() const
{
    return capturing_.load();
}

void FileCapture::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    frameCallback_ = callback;
}

std::string FileCapture::getConfiguration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json config;
    config["type"] = "VIDEO_FILE";
    config["id"] = config_.id;
    config["name"] = config_.name;
    config["sourcePath"] = config_.sourcePath;
    config["width"] = config_.width;
    config["height"] = config_.height;
    config["frameRate"] = config_.frameRate;
    config["pixelFormat"] = config_.pixelFormat;
    config["looping"] = looping_;
    config["duration"] = fileDuration_;
    
    return config.dump();
}

FrameSourceType FileCapture::getSourceType() const
{
    return FrameSourceType::VIDEO_FILE;
}

std::string FileCapture::getSourceName() const
{
    return "FileCapture";
}

std::string FileCapture::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    stats["type"] = "VIDEO_FILE";
    stats["capturing"] = capturing_.load();
    stats["framesGenerated"] = framesGenerated_;
    stats["bytesGenerated"] = bytesGenerated_;
    stats["currentPosition"] = currentPosition_;
    stats["duration"] = fileDuration_;
    stats["looping"] = looping_;
    stats["filePath"] = config_.sourcePath;
    
    if (startTime_ > 0)
    {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        stats["uptimeSeconds"] = (now - startTime_) / 1000.0;
    }
    
    return stats.dump();
}

bool FileCapture::updateConfiguration(const FrameSourceConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    if (config.loop != looping_)
    {
        looping_ = config.loop;
    }
    
    if (config.frameRate > 0)
    {
        config_.frameRate = config.frameRate;
    }
    
    LOG_INFO("FileCapture configuration updated");
    return true;
}

bool FileCapture::supportsSeeking() const
{
    return true; // Video files support seeking
}

bool FileCapture::seek(int64_t timestamp)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (timestamp < 0 || timestamp > fileDuration_)
    {
        LOG_WARN("Invalid seek position: {}ms (duration: {}ms)", timestamp, fileDuration_);
        return false;
    }
    
    currentPosition_ = timestamp;
    LOG_INFO("FileCapture seeked to: {}ms", timestamp);
    return true;
}

int64_t FileCapture::getCurrentPosition() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return currentPosition_;
}

int64_t FileCapture::getDuration() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fileDuration_;
}

void FileCapture::cleanup()
{
    stopCapture();
    
#ifdef JETSON_ENABLE_STREAMING
    cleanupFFmpeg();
#endif
    
    std::lock_guard<std::mutex> lock(mutex_);
    fileData_.clear();
    framesGenerated_ = 0;
    bytesGenerated_ = 0;
    startTime_ = 0;
    currentPosition_ = 0;
    
    LOG_INFO("FileCapture cleaned up");
}

bool FileCapture::setLooping(bool loop)
{
    std::lock_guard<std::mutex> lock(mutex_);
    looping_ = loop;
    LOG_INFO("FileCapture looping set to: {}", loop);
    return true;
}

bool FileCapture::isLooping() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return looping_;
}

std::string FileCapture::getFilePath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sourcePath;
}

void FileCapture::captureLoop()
{
    const auto frameDuration = std::chrono::milliseconds(1000 / config_.frameRate);
    
#ifdef JETSON_ENABLE_STREAMING
    if (ffmpegInitialized_ && formatContext_)
    {
        // FFmpeg mode: read frames sequentially from the video file
        AVPacket* packet = av_packet_alloc();
        AVFrame* decodedFrame = av_frame_alloc();
        
        while (!shouldStop_)
        {
            auto frameStart = std::chrono::steady_clock::now();
            
            // Read packet from file
            int ret = av_read_frame(formatContext_, packet);
            if (ret < 0)
            {
                // End of file or error
                if (looping_)
                {
                    // Seek back to start
                    av_seek_frame(formatContext_, -1, 0, AVSEEK_FLAG_BACKWARD);
                    currentPosition_ = 0;
                    LOG_INFO("FileCapture looping to start");
                    continue;
                }
                else
                {
                    LOG_INFO("FileCapture reached end of file");
                    break;
                }
            }
            
            // Only process video packets
            if (packet->stream_index == videoStreamIndex_)
            {
                // Send packet to decoder
                ret = avcodec_send_packet(codecContext_, packet);
                if (ret == 0)
                {
                    // Receive frame from decoder
                    ret = avcodec_receive_frame(codecContext_, decodedFrame);
                    if (ret == 0)
                    {
                        // Convert decoded frame to RGB
                        frame_ = decodedFrame;
                        if (convertFrameToRGB())
                        {
                            // Create video frame
                            VideoFrame frame;
                            frame.width = config_.width;
                            frame.height = config_.height;
                            frame.timestamp = currentPosition_;
                            frame.codec = config_.pixelFormat;
                            
                            // Copy frame data
                            size_t frameSize = config_.width * config_.height * 3;
                            frame.data.resize(frameSize);
                            
                            if (rgbFrame_ && rgbFrame_->data[0])
                            {
                                std::memcpy(frame.data.data(), rgbFrame_->data[0], frameSize);
                                
                                // Update statistics
                                framesGenerated_++;
                                bytesGenerated_ += frame.data.size();
                                currentPosition_ += (1000 / config_.frameRate);
                                
                                // Deliver frame via callback
                                if (frameCallback_)
                                {
                                    frameCallback_(frame);
                                }
                            }
                        }
                    }
                }
            }
            
            av_packet_unref(packet);
            
            // Maintain frame rate
            auto frameEnd = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
            
            if (elapsed < frameDuration)
            {
                std::this_thread::sleep_for(frameDuration - elapsed);
            }
        }
        
        av_packet_free(&packet);
        av_frame_free(&decodedFrame);
    }
    else
    {
#endif
        // Legacy mode: generate placeholder frames
        while (!shouldStop_)
        {
            auto frameStart = std::chrono::steady_clock::now();
            
            // Generate frame
            VideoFrame frame = generateFrame();
            
            // Update statistics
            framesGenerated_++;
            bytesGenerated_ += frame.data.size();
            currentPosition_ += (1000 / config_.frameRate);
            
            // Handle looping
            if (currentPosition_ >= fileDuration_ && fileDuration_ > 0)
            {
                if (looping_)
                {
                    currentPosition_ = 0;
                    LOG_INFO("FileCapture looping to start");
                }
                else
                {
                    LOG_INFO("FileCapture reached end of file");
                    break;
                }
            }
            
            // Deliver frame via callback
            if (frameCallback_)
            {
                frameCallback_(frame);
            }
            
            // Maintain frame rate
            auto frameEnd = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
            
            if (elapsed < frameDuration)
            {
                std::this_thread::sleep_for(frameDuration - elapsed);
            }
        }
#ifdef JETSON_ENABLE_STREAMING
    }
#endif
    
    capturing_ = false;
}

VideoFrame FileCapture::generateFrame()
{
    // Extract frame at current position
    return extractFrame(currentPosition_);
}

bool FileCapture::loadVideoFile()
{
    // Check if file exists
    if (!std::filesystem::exists(config_.sourcePath))
    {
        LOG_ERROR("Video file not found: {}", config_.sourcePath);
        return false;
    }
    
#ifdef JETSON_ENABLE_STREAMING
    // Try to load with FFmpeg first
    if (loadVideoFileFFmpeg())
    {
        LOG_INFO("Video file loaded with FFmpeg: {}", config_.sourcePath);
        return true;
    }
    else
    {
        LOG_WARN("FFmpeg loading failed, falling back to legacy mode");
    }
#endif
    
    // Fallback to legacy mode
    uintmax_t fileSize = std::filesystem::file_size(config_.sourcePath);
    
    fileData_.resize(fileSize);
    
    std::ifstream file(config_.sourcePath, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(fileData_.data()), fileSize))
    {
        LOG_ERROR("Failed to read video file: {}", config_.sourcePath);
        return false;
    }
    
    // Calculate duration based on file size and frame rate
    // This is a simplified calculation
    fileDuration_ = calculateDuration();
    
    LOG_INFO("Loaded video file (legacy mode): {} ({} bytes, estimated duration: {}ms)", 
             config_.sourcePath, fileSize, fileDuration_);
    
    return true;
}

VideoFrame FileCapture::extractFrame(int64_t timestamp)
{
#ifdef JETSON_ENABLE_STREAMING
    if (ffmpegInitialized_ && formatContext_ && codecContext_)
    {
        // Use FFmpeg to decode the actual frame
        AVPacket* packet = av_packet_alloc();
        AVFrame* decodedFrame = av_frame_alloc();
        
        VideoFrame resultFrame;
        resultFrame.width = config_.width;
        resultFrame.height = config_.height;
        resultFrame.timestamp = timestamp;
        resultFrame.codec = config_.pixelFormat;
        
        // Seek to the approximate position
        int64_t seekTarget = timestamp * AV_TIME_BASE / 1000; // Convert ms to AV time base
        av_seek_frame(formatContext_, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        
        // Read frames until we get one at the right position
        bool frameFound = false;
        while (av_read_frame(formatContext_, packet) >= 0)
        {
            if (packet->stream_index == videoStreamIndex_)
            {
                // Send packet to decoder
                int ret = avcodec_send_packet(codecContext_, packet);
                if (ret == 0)
                {
                    // Receive frame from decoder
                    ret = avcodec_receive_frame(codecContext_, decodedFrame);
                    if (ret == 0)
                    {
                        // Convert decoded frame to RGB
                        frame_ = decodedFrame; // Use the decoded frame
                        if (convertFrameToRGB())
                        {
                            // Copy frame data to result
                            size_t frameSize = config_.width * config_.height * 3;
                            resultFrame.data.resize(frameSize);
                            
                            // Copy from rgbFrame_
                            if (rgbFrame_ && rgbFrame_->data[0])
                            {
                                std::memcpy(resultFrame.data.data(), rgbFrame_->data[0], frameSize);
                                frameFound = true;
                                break;
                            }
                        }
                    }
                }
            }
            av_packet_unref(packet);
            
            if (frameFound) break;
        }
        
        av_packet_free(&packet);
        av_frame_free(&decodedFrame);
        
        if (frameFound)
        {
            return resultFrame;
        }
    }
#endif
    
    // Fallback to placeholder frame
    VideoFrame frame;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.timestamp = timestamp;
    frame.codec = config_.pixelFormat;
    
    size_t frameSize = config_.width * config_.height * 3; // RGB24
    frame.data.resize(frameSize, 128); // Fill with gray
    
    // Add some variation based on timestamp to simulate different frames
    uint8_t variation = static_cast<uint8_t>((timestamp / 100) % 256);
    for (size_t i = 0; i < frame.data.size(); i += 3)
    {
        frame.data[i] = variation;
        frame.data[i + 1] = 128;
        frame.data[i + 2] = 255 - variation;
    }
    
    return frame;
}

int64_t FileCapture::calculateDuration() const
{
#ifdef JETSON_ENABLE_STREAMING
    if (ffmpegInitialized_ && formatContext_)
    {
        // Get actual duration from FFmpeg
        return (formatContext_->duration * 1000) / AV_TIME_BASE; // Convert to milliseconds
    }
#endif
    
    // Simplified duration calculation
    // In a real implementation, this would come from video metadata
    int64_t estimatedFrames = fileData_.size() / (config_.width * config_.height * 3);
    return (estimatedFrames * 1000) / config_.frameRate;
}

#ifdef JETSON_ENABLE_STREAMING
bool FileCapture::loadVideoFileFFmpeg()
{
    LOG_INFO("Starting FFmpeg video file loading: {}", config_.sourcePath);
    
    // Open video file
    LOG_INFO("Opening video file with FFmpeg...");
    if (avformat_open_input(&formatContext_, config_.sourcePath.c_str(), nullptr, nullptr) != 0)
    {
        LOG_ERROR("Could not open video file: {}", config_.sourcePath);
        return false;
    }
    LOG_INFO("Video file opened successfully");
    
    // Retrieve stream information
    LOG_INFO("Retrieving stream information...");
    if (avformat_find_stream_info(formatContext_, nullptr) < 0)
    {
        LOG_ERROR("Could not find stream information");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Stream information retrieved successfully");
    
    // Find the first video stream
    LOG_INFO("Finding video stream...");
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; i++)
    {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex_ = i;
            break;
        }
    }
    
    if (videoStreamIndex_ == -1)
    {
        LOG_ERROR("Could not find video stream");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Video stream found at index: {}", videoStreamIndex_);
    
    // Get the codec
    LOG_INFO("Finding codec...");
    AVCodecParameters* codecPar = formatContext_->streams[videoStreamIndex_]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec)
    {
        LOG_ERROR("Unsupported codec");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Codec found: {}", codec->name);
    
    // Create codec context
    LOG_INFO("Allocating codec context...");
    codecContext_ = avcodec_alloc_context3(codec);
    if (!codecContext_)
    {
        LOG_ERROR("Could not allocate codec context");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Codec context allocated");
    
    // Copy codec parameters
    LOG_INFO("Copying codec parameters...");
    if (avcodec_parameters_to_context(codecContext_, codecPar) < 0)
    {
        LOG_ERROR("Could not copy codec parameters");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Codec parameters copied");
    
    // Open codec
    LOG_INFO("Opening codec...");
    if (avcodec_open2(codecContext_, codec, nullptr) < 0)
    {
        LOG_ERROR("Could not open codec");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Codec opened successfully");
    
    // Update config with actual video properties
    config_.width = codecContext_->width;
    config_.height = codecContext_->height;
    
    // Calculate frame rate from stream
    AVStream* videoStream = formatContext_->streams[videoStreamIndex_];
    if (videoStream->avg_frame_rate.den > 0)
    {
        config_.frameRate = videoStream->avg_frame_rate.num / videoStream->avg_frame_rate.den;
    }
    
    // Get duration
    fileDuration_ = (formatContext_->duration * 1000) / AV_TIME_BASE;
    
    LOG_INFO("Video properties: {}x{} @ {}fps, duration: {}ms", 
             config_.width, config_.height, config_.frameRate, fileDuration_);
    
    // Allocate frames
    LOG_INFO("Allocating frames...");
    frame_ = av_frame_alloc();
    rgbFrame_ = av_frame_alloc();
    
    if (!frame_ || !rgbFrame_)
    {
        LOG_ERROR("Could not allocate frames");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Frames allocated");
    
    // Set up RGB frame
    rgbFrame_->format = AV_PIX_FMT_RGB24;
    rgbFrame_->width = config_.width;
    rgbFrame_->height = config_.height;
    
    LOG_INFO("Allocating RGB frame buffer...");
    if (av_frame_get_buffer(rgbFrame_, 0) < 0)
    {
        LOG_ERROR("Could not allocate RGB frame buffer");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("RGB frame buffer allocated");
    
    // Set up scaler
    LOG_INFO("Initializing scaler...");
    swsContext_ = sws_getContext(
        codecContext_->width, codecContext_->height, codecContext_->pix_fmt,
        config_.width, config_.height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!swsContext_)
    {
        LOG_ERROR("Could not initialize scaler");
        cleanupFFmpeg();
        return false;
    }
    LOG_INFO("Scaler initialized");
    
    ffmpegInitialized_ = true;
    
    LOG_INFO("FFmpeg video loaded successfully: {} ({}x{} @ {}fps, duration: {}ms)", 
             config_.sourcePath, config_.width, config_.height, config_.frameRate, fileDuration_);
    
    return true;
}

bool FileCapture::convertFrameToRGB()
{
    if (!frame_ || !rgbFrame_ || !swsContext_)
    {
        return false;
    }
    
    // Scale and convert to RGB
    sws_scale(swsContext_,
              frame_->data, frame_->linesize, 0, frame_->height,
              rgbFrame_->data, rgbFrame_->linesize);
    
    return true;
}

void FileCapture::cleanupFFmpeg()
{
    if (swsContext_)
    {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }
    
    if (rgbFrame_)
    {
        av_frame_free(&rgbFrame_);
        rgbFrame_ = nullptr;
    }
    
    if (frame_)
    {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
    
    if (codecContext_)
    {
        avcodec_free_context(&codecContext_);
        codecContext_ = nullptr;
    }
    
    if (formatContext_)
    {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
    }
    
    ffmpegInitialized_ = false;
    
    LOG_INFO("FFmpeg resources cleaned up");
}
#endif

} // namespace embed::bmcweb::streaming