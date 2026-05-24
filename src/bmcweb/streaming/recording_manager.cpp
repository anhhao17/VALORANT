#include "recording_manager.hpp"
#include "../logging.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace embed::bmcweb::streaming
{

RecordingManager::RecordingManager() : recordingPath_("/tmp/jetson_recordings"), recordingEnabled_(false)
{
    LOG_INFO("Recording manager initialized");
    
    // Create recording directory
    try
    {
        std::filesystem::create_directories(recordingPath_);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create recording directory: {}", e.what());
    }
}

RecordingManager::~RecordingManager()
{
    // Stop all recording threads
    for (auto& [id, thread] : recordingThreads_)
    {
        if (thread.joinable())
        {
            auto it = recordings_.find(id);
            if (it != recordings_.end())
            {
                it->second.state = RecordingState::STOPPED;
            }
            thread.join();
        }
    }
    
    LOG_INFO("Recording manager shutdown");
}

std::string RecordingManager::startRecording(const std::string& streamId, const std::string& format, StreamSourceType streamType)
{
    if (!recordingEnabled_)
    {
        LOG_WARN("Recording is disabled");
        return "";
    }
    
    // Don't allow recording from file-based streams (only from live sources)
    if (streamType == StreamSourceType::MP4_FILE)
    {
        LOG_WARN("Recording is not supported for file-based streams");
        return "";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate recording ID and path
    std::string recordingId = generateRecordingId();
    std::string filePath = generateRecordingPath(streamId, format);
    
    // Create recording info
    RecordingInfo info;
    info.recordingId = recordingId;
    info.streamId = streamId;
    info.filePath = filePath;
    info.state = RecordingState::RECORDING;
    info.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.duration = 0;
    info.fileSize = 0;
    info.format = format;
    
    recordings_[recordingId] = info;
    
    // Start recording thread
    recordingThreads_[recordingId] = std::thread(&RecordingManager::recordingThread, this, recordingId, streamId);
    
    LOG_INFO("Recording started: {} for stream: {}", recordingId, streamId);
    return recordingId;
}

bool RecordingManager::stopRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    // Update state
    it->second.state = RecordingState::STOPPED;
    
    // Calculate final duration
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    it->second.duration = now - it->second.startTime;
    
    // Stop recording thread
    if (recordingThreads_[recordingId].joinable())
    {
        recordingThreads_[recordingId].join();
    }
    
    // Get final file size
    try
    {
        std::filesystem::path filePath(it->second.filePath);
        if (std::filesystem::exists(filePath))
        {
            it->second.fileSize = std::filesystem::file_size(filePath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to get recording file size: {}", e.what());
    }
    
    LOG_INFO("Recording stopped: {} (duration: {}ms, size: {} bytes)", 
             recordingId, it->second.duration, it->second.fileSize);
    return true;
}

bool RecordingManager::pauseRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    if (it->second.state != RecordingState::RECORDING)
    {
        LOG_WARN("Recording is not in recording state: {}", recordingId);
        return false;
    }
    
    it->second.state = RecordingState::PAUSED;
    LOG_INFO("Recording paused: {}", recordingId);
    return true;
}

bool RecordingManager::resumeRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    if (it->second.state != RecordingState::PAUSED)
    {
        LOG_WARN("Recording is not in paused state: {}", recordingId);
        return false;
    }
    
    it->second.state = RecordingState::RECORDING;
    LOG_INFO("Recording resumed: {}", recordingId);
    return true;
}

RecordingInfo RecordingManager::getRecordingInfo(const std::string& recordingId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it != recordings_.end())
    {
        return it->second;
    }
    
    return RecordingInfo{};
}

std::vector<RecordingInfo> RecordingManager::getAllRecordings() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RecordingInfo> result;
    for (const auto& [id, info] : recordings_)
    {
        result.push_back(info);
    }
    
    return result;
}

std::vector<RecordingInfo> RecordingManager::getStreamRecordings(const std::string& streamId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RecordingInfo> result;
    for (const auto& [id, info] : recordings_)
    {
        if (info.streamId == streamId)
        {
            result.push_back(info);
        }
    }
    
    return result;
}

bool RecordingManager::deleteRecording(const std::string& recordingId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = recordings_.find(recordingId);
    if (it == recordings_.end())
    {
        LOG_WARN("Recording not found: {}", recordingId);
        return false;
    }
    
    // Stop recording if still active
    if (it->second.state == RecordingState::RECORDING)
    {
        it->second.state = RecordingState::STOPPED;
        if (recordingThreads_[recordingId].joinable())
        {
            recordingThreads_[recordingId].join();
        }
    }
    
    // Delete file
    try
    {
        if (std::filesystem::exists(it->second.filePath))
        {
            std::filesystem::remove(it->second.filePath);
            LOG_INFO("Recording file deleted: {}", it->second.filePath);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to delete recording file: {}", e.what());
    }
    
    // Remove from recordings map
    recordings_.erase(it);
    recordingThreads_.erase(recordingId);
    
    LOG_INFO("Recording deleted: {}", recordingId);
    return true;
}

void RecordingManager::setRecordingPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    recordingPath_ = path;
    LOG_INFO("Recording path set to: {}", path);
}

void RecordingManager::setRecordingEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    recordingEnabled_ = enabled;
    LOG_INFO("Recording enabled: {}", enabled);
}

std::string RecordingManager::getRecordingPath() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return recordingPath_;
}

bool RecordingManager::isRecordingEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return recordingEnabled_;
}

void RecordingManager::recordingThread(const std::string& recordingId, const std::string& streamId)
{
    LOG_INFO("Recording thread started for: {}", recordingId);
    
    while (true)
    {
        RecordingState currentState;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = recordings_.find(recordingId);
            if (it == recordings_.end() || it->second.state == RecordingState::STOPPED)
            {
                break;
            }
            currentState = it->second.state;
        }
        
        if (currentState == RecordingState::PAUSED)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Recording logic would go here
        // For now, we'll just sleep to simulate recording
        // In production, this would use FFmpeg or similar to record the stream
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LOG_INFO("Recording thread stopped for: {}", recordingId);
}

std::string RecordingManager::generateRecordingId()
{
    // Generate unique recording ID based on timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return "rec_" + std::to_string(timestamp);
}

std::string RecordingManager::generateRecordingPath(const std::string& streamId, const std::string& format)
{
    // Create recording directory if it doesn't exist
    std::filesystem::create_directories(recordingPath_);
    
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    std::string filename = streamId + "_" + std::to_string(timestamp) + "." + format;
    return recordingPath_ + "/" + filename;
}

} // namespace embed::bmcweb::streaming