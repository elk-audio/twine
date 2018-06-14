#include <thread>
#include <functional>
#include <iostream>

#include "gtest/gtest.h"

#include "std_worker_pool.cpp"

using namespace twine;

void test_function(bool& running, bool& flag, BarrierWithTrigger& barrier)
{
    while (running)
    {
         barrier.wait();
         flag = true;
    }
}


TEST (BarrierTest, TestBarrierWithTrigger)
{
    bool a = false;
    bool b = false;
    bool running = true;

    BarrierWithTrigger module_under_test;
    module_under_test.set_no_threads(2);
    std::thread t1(test_function, std::ref(running), std::ref(a), std::ref(module_under_test));
    std::thread t2(test_function, std::ref(running), std::ref(b), std::ref(module_under_test));
    /* threads should start in wait mode */
    module_under_test.wait_for_all();
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);

    /* Run both threads and wait for them to stop at the barrier again */
    module_under_test.relase_all();

    module_under_test.wait_for_all();

    /* Both flags should now be set to true */
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);

    running = false;
    module_under_test.relase_all();

    t1.join();
    t2.join();
}

// A more complex test case where tests can be grouped
// And setup and teardown functions added.
class StdWorkerPoolTest : public ::testing::Test
{
protected:
    StdWorkerPoolTest() {}

    StdWorkerPool _module_under_test{2};
    bool a{false};
    bool b{false};
};

void worker_function(void* data)
{
    bool* flag = reinterpret_cast<bool*>(data);
    *flag = true;
}


TEST_F(StdWorkerPoolTest, FunctionalityTest)
{
    _module_under_test.add_worker(worker_function, &a);
    _module_under_test.add_worker(worker_function, &b);

    _module_under_test.wait_for_workers_idle();
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);

    _module_under_test.wakeup_workers();

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
}