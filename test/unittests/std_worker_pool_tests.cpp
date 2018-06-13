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
