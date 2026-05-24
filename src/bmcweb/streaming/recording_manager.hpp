#pragma once

#include "stream_types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace embed::bmcweb::streaming
{

/**
 * @brief Recording manager for handling stream recordings
 * 
 * Manages recording lifecycle, file operations, and recording threads.
 */
class RecordingManager
{
   public:
    static RecordingManager& getInstance()
    {
        static RecordingManager instance;
        return instance;
    }
    
    // Recording operations
    std::string startRecording(const std::string& streamId, const std::string& format = "mp4");
    bool stopRecording(const std::string& recordingId);
    bool pauseRecording(const std::string& recordingId);
    bool resumeRecording(const std::string& recordingId);
    
    // Recording queries
    RecordingInfo getRecordingInfo(const std::string& recordingId) const;
    std::vector<RecordingInfo> getAllRecordings() const;
    std::vector<RecordingInfo> getStreamRecordings(const std::string& streamId) const;
    bool deleteRecording(const std::string& recordingId);
    
    // Configuration
    void setRecordingPath(const std::string& path);
    void setRecordingEnabled(bool enabled);
    std::string getRecordingPath() const;
    bool isRecordingEnabled() const;
    
   private:
    RecordingManager();
    ~RecordingManager();
    
    void recordingThread(const std::string& recordingId, const std::string& streamId);
    std::string generateRecordingId();
    std::string generateRecordingPath(const std::string& streamId, const std::string& format);
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RecordingInfo> recordings_;
    std::unordered_map<std::string, std::thread> recordingThreads_;
    std::string recordingPath_;
    bool recordingEnabled_;
};

} // namespace embed::bmcweb::streaming