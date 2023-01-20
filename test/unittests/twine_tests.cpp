#include <thread>

#include "gtest/gtest.h"

#include "twine.cpp"
#include "thread_helpers_posix.cpp"
#ifdef TWINE_BUILD_WITH_XENOMAI
#include "thread_helpers_xenomai.cpp"
#endif
#include "twine_version.h"

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

TEST (TwineTest, TestVersionAndBuildInfo)
{
    auto version = twine::twine_version();
    EXPECT_EQ(TWINE__VERSION_MAJ, version.major);
    EXPECT_EQ(TWINE__VERSION_MIN, version.minor);
    EXPECT_EQ(TWINE__VERSION_REV, version.revision);
    EXPECT_GT(strlen(twine::build_info()), 100u);
}
