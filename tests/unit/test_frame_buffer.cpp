#include <chrono>
#include <gtest/gtest.h>
#include <streaming/Frame.h>
#include <streaming/FrameBuffer.h>
#include <thread>

using namespace jetson::streaming;

TEST(FrameBuffer, BasicPushPop)
{
    FrameBuffer buffer(10);

    auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
    frame->sequence = 1;

    ASSERT_TRUE(buffer.try_push(frame));
    ASSERT_EQ(buffer.size(), 1);

    FramePtr popped_frame;
    ASSERT_TRUE(buffer.try_pop(popped_frame));
    ASSERT_EQ(popped_frame->sequence, 1);
    ASSERT_EQ(buffer.size(), 0);
}

TEST(FrameBuffer, CapacityLimit)
{
    FrameBuffer buffer(3);

    for (int i = 0; i < 3; ++i)
    {
        auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
        frame->sequence = i;
        ASSERT_TRUE(buffer.try_push(frame));
    }

    // Buffer should be full
    ASSERT_TRUE(buffer.full());

    // Try to push when full
    auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
    ASSERT_FALSE(buffer.try_push(frame));
}

TEST(FrameBuffer, BlockingPushPop)
{
    FrameBuffer buffer(1);

    auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
    frame->sequence = 42;

    // Push in separate thread
    std::thread pusher([&buffer, frame]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        buffer.push(frame);
    });

    // Pop should block until frame is available
    FramePtr popped_frame;
    ASSERT_TRUE(buffer.pop(popped_frame));
    ASSERT_EQ(popped_frame->sequence, 42);

    pusher.join();
}

TEST(FrameBuffer, Close)
{
    FrameBuffer buffer(10);

    buffer.close();
    ASSERT_TRUE(buffer.is_closed());

    auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
    ASSERT_FALSE(buffer.try_push(frame));

    FramePtr popped_frame;
    ASSERT_FALSE(buffer.try_pop(popped_frame));
}

TEST(FrameBuffer, Clear)
{
    FrameBuffer buffer(10);

    for (int i = 0; i < 5; ++i)
    {
        auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
        buffer.try_push(frame);
    }

    ASSERT_EQ(buffer.size(), 5);

    buffer.clear();
    ASSERT_EQ(buffer.size(), 0);
    ASSERT_TRUE(buffer.empty());
}

TEST(FrameBuffer, ThreadSafety)
{
    FrameBuffer buffer(100);
    const int num_producers = 4;
    const int num_consumers = 4;
    const int frames_per_producer = 25;

    std::atomic<int> total_produced{0};
    std::atomic<int> total_consumed{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Producer threads
    for (int i = 0; i < num_producers; ++i)
    {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < frames_per_producer; ++j)
            {
                auto frame = std::make_shared<Frame>(640, 480, PixelFormat::RGB24);
                frame->sequence = i * frames_per_producer + j;
                buffer.push(frame);
                total_produced++;
            }
        });
    }

    // Consumer threads
    for (int i = 0; i < num_consumers; ++i)
    {
        consumers.emplace_back([&]() {
            for (int j = 0; j < frames_per_producer; ++j)
            {
                FramePtr frame;
                buffer.pop(frame);
                total_consumed++;
            }
        });
    }

    for (auto& t : producers)
        t.join();
    for (auto& t : consumers)
        t.join();

    ASSERT_EQ(total_produced, num_producers * frames_per_producer);
    ASSERT_EQ(total_consumed, num_producers * frames_per_producer);
    ASSERT_EQ(buffer.size(), 0);
}

TEST(Frame, BytesPerPixel)
{
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::RGB24), 3);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::BGR24), 3);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::RGBA32), 4);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::BGRA32), 4);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::GRAY8), 1);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::YUYV422), 2);
    EXPECT_EQ(Frame::get_bytes_per_pixel(PixelFormat::UNKNOWN), 0);
}

TEST(Frame, Construction)
{
    Frame frame(640, 480, PixelFormat::RGB24);

    ASSERT_EQ(frame.width, 640);
    ASSERT_EQ(frame.height, 480);
    ASSERT_EQ(frame.format, PixelFormat::RGB24);
    ASSERT_NE(frame.data, nullptr);
    ASSERT_EQ(frame.size, 640 * 480 * 3);
}

TEST(Frame, MoveSemantics)
{
    Frame frame1(640, 480, PixelFormat::RGB24);
    uint8_t* original_data = frame1.data;

    Frame frame2 = std::move(frame1);

    ASSERT_EQ(frame2.data, original_data);
    ASSERT_EQ(frame1.data, nullptr);
    ASSERT_EQ(frame1.size, 0);
}
