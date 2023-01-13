#include <thread>

#include "gtest/gtest.h"

#include "twine/twine.h"

using namespace twine;

namespace twine {
int  get_next_id();
void deregister_id(int id);
}

std::atomic_bool flag = false;

void test_function(RtConditionVariable* cond_var)
{
    flag = cond_var->wait();
}

class RtConditionVariableTest : public ::testing::Test
{
protected:
    RtConditionVariableTest() {}

    void SetUp()
    {
        _module_under_test = RtConditionVariable::create_rt_condition_variable();
        ASSERT_NE(nullptr, _module_under_test);
    }

    std::unique_ptr<RtConditionVariable> _module_under_test;
};

TEST_F(RtConditionVariableTest, FunctionalityTest)
{
    flag = false;
    std::thread thread(test_function, _module_under_test.get());

    EXPECT_FALSE(flag);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_FALSE(flag);
    _module_under_test->notify();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_TRUE(flag);
    thread.join();
}

#ifdef TWINE_BUILD_XENOMAI_TESTS
TEST(IdGenerationTest, TestOrder)
{
    ASSERT_EQ(0, get_next_id());
    ASSERT_EQ(1, get_next_id());
    ASSERT_EQ(2, get_next_id());
    deregister_id(1);
    ASSERT_EQ(1, get_next_id());
    ASSERT_EQ(3, get_next_id());

    EXPECT_THROW( for(int i = 0; i < 100; ++i)
                  {
                      get_next_id();
                  },
                  std::runtime_error);
}
#endif