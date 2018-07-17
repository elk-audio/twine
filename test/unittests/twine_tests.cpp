#include <thread>

#include "gtest/gtest.h"

#include "twine.cpp"

using namespace twine;

TEST (TwineTest, TestThreadRtFlag)
{
    ASSERT_FALSE(is_current_thread_realtime());
    /* With a ThreadRtFlag on the stack it should report true */
    ThreadRtFlag rt_flag;
    ASSERT_TRUE(is_current_thread_realtime());

    /* But from another thread it should still be false */
    std::thread test_thread([](){ASSERT_FALSE(is_current_thread_realtime());});
    test_thread.join();
}

TEST (TwineTest, TestRtTimestamp)
{
    auto time_1 = current_rt_time();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    auto time_2 = current_rt_time();
    ASSERT_GT(time_2, time_1);
}