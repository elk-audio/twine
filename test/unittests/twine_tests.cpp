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
