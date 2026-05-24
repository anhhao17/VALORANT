#include <chrono>
#include <gtest/gtest.h>
#include <memory>

#include "streaming/FileVideoSource.h"
#include "streaming/IVideoSource.h"
#include "streaming/TestPatternSource.h"

using namespace jetson::streaming;

class VideoSourceTest : public ::testing::Test
{
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

// TestPatternSource tests
TEST_F(VideoSourceTest, TestPatternSourceConstruction)
{
    TestPatternSource source(1280, 720, 30.0);
    EXPECT_EQ(source.width(), 1280);
    EXPECT_EQ(source.height(), 720);
    EXPECT_EQ(source.fps(), 30.0);
}

TEST_F(VideoSourceTest, TestPatternSourceOpenClose)
{
    TestPatternSource source(640, 480, 30.0);
    EXPECT_TRUE(source.open());
    EXPECT_TRUE(source.is_open());
    source.close();
    EXPECT_FALSE(source.is_open());
}

TEST_F(VideoSourceTest, TestPatternSourceReadFrame)
{
    TestPatternSource source(640, 480, 30.0);
    ASSERT_TRUE(source.open());

    FramePtr frame;
    EXPECT_TRUE(source.read_frame(frame));
    EXPECT_GT(frame->width, 0);
    EXPECT_GT(frame->height, 0);
    EXPECT_EQ(frame->width, 640);
    EXPECT_EQ(frame->height, 480);
    EXPECT_GT(frame->size, 0);
    EXPECT_NE(frame->data, nullptr);

    source.close();
}

TEST_F(VideoSourceTest, TestPatternSourceMultipleFrames)
{
    TestPatternSource source(640, 480, 30.0);
    ASSERT_TRUE(source.open());

    FramePtr frame1, frame2, frame3;
    EXPECT_TRUE(source.read_frame(frame1));
    EXPECT_TRUE(source.read_frame(frame2));
    EXPECT_TRUE(source.read_frame(frame3));

    // Frames should have data
    EXPECT_GT(frame1->size, 0);
    EXPECT_NE(frame1->data, nullptr);
    EXPECT_GT(frame2->size, 0);
    EXPECT_NE(frame2->data, nullptr);
    EXPECT_GT(frame3->size, 0);
    EXPECT_NE(frame3->data, nullptr);

    source.close();
}

// FileVideoSource tests
TEST_F(VideoSourceTest, FileVideoSourceConstruction)
{
    FileVideoSource source("test.mp4");
    // FileVideoSource constructor takes file_path, width, height, target_fps
    // Just verify it can be constructed
}

TEST_F(VideoSourceTest, FileVideoSourceInvalidPath)
{
    FileVideoSource source("/nonexistent/file.mp4");
    EXPECT_FALSE(source.open());
    EXPECT_FALSE(source.is_open());
}

TEST_F(VideoSourceTest, FileVideoSourceOpenClose)
{
    // This test requires a valid video file
    // Skip if no test video is available
    FileVideoSource source("test.mp4");
    if (!source.open())
    {
        GTEST_SKIP() << "Test video file not available, skipping FileVideoSource test";
    }

    EXPECT_TRUE(source.is_open());
    source.close();
    EXPECT_FALSE(source.is_open());
}

// Frame validation tests
TEST_F(VideoSourceTest, FrameValidation)
{
    TestPatternSource source(640, 480, 30.0);
    ASSERT_TRUE(source.open());

    FramePtr frame;
    ASSERT_TRUE(source.read_frame(frame));

    // Validate frame properties
    EXPECT_GT(frame->width, 0);
    EXPECT_GT(frame->height, 0);
    EXPECT_GE(frame->timestamp, 0);  // First frame may have timestamp 0
    EXPECT_GT(frame->size, 0);
    EXPECT_NE(frame->data, nullptr);

    // For RGB24 format, data size should be width * height * 3
    size_t expected_size = frame->width * frame->height * 3;
    EXPECT_EQ(frame->size, expected_size);

    source.close();
}

// Source switching tests
TEST_F(VideoSourceTest, SourceSwitching)
{
    TestPatternSource source1(640, 480, 30.0);
    TestPatternSource source2(1280, 720, 30.0);

    ASSERT_TRUE(source1.open());
    EXPECT_EQ(source1.width(), 640);

    source1.close();
    ASSERT_TRUE(source2.open());
    EXPECT_EQ(source2.width(), 1280);

    source2.close();
}

// Performance test
TEST_F(VideoSourceTest, TestPatternSourcePerformance)
{
    TestPatternSource source(640, 480, 30.0);
    ASSERT_TRUE(source.open());

    const int frame_count = 100;
    FramePtr frame;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < frame_count; ++i)
    {
        ASSERT_TRUE(source.read_frame(frame));
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double fps = (frame_count * 1000.0) / duration.count();

    // Test pattern should be fast (> 200 fps) — avoids flakiness on slow CI runners
    EXPECT_GT(fps, 200.0);

    source.close();
}
