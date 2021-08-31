#include <thread>
#include <functional>
#include <iostream>

#include "gtest/gtest.h"

#define private public
#include "worker_pool_implementation.h"

using namespace twine;

constexpr int N_TEST_WORKERS = 4;

void test_function(bool& running, bool& flag, BarrierWithTrigger<ThreadType::PTHREAD>(& barrier))
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

    BarrierWithTrigger<ThreadType::PTHREAD> module_under_test;
    module_under_test.set_no_threads(2);
    std::thread t1(test_function, std::ref(running), std::ref(a), std::ref(module_under_test));
    std::thread t2(test_function, std::ref(running), std::ref(b), std::ref(module_under_test));
    /* threads should start in wait mode */
    module_under_test.wait_for_all();
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);

    /* Run both threads and wait for them to stop at the barrier again */
    module_under_test.release_all();

    module_under_test.wait_for_all();

    /* Both flags should now be set to true */
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);

    running = false;
    module_under_test.release_all();

    t1.join();
    t2.join();
}

class PthreadWorkerPoolTest : public ::testing::Test
{
protected:
    PthreadWorkerPoolTest() {}

    WorkerPoolImpl<ThreadType::PTHREAD> _module_under_test{N_TEST_WORKERS, true};
    bool a{false};
    bool b{false};
};

void worker_function(void* data)
{
    bool* flag = reinterpret_cast<bool*>(data);
    *flag = true;
}

TEST_F(PthreadWorkerPoolTest, FunctionalityTest)
{
    auto res = _module_under_test.add_worker(worker_function, &a);
    ASSERT_EQ(WorkerPoolStatus::OK, res);
    res = _module_under_test.add_worker(worker_function, &b);
    ASSERT_EQ(WorkerPoolStatus::OK, res);
    ASSERT_FALSE(a);
    ASSERT_FALSE(b);
    _module_under_test.wakeup_workers();
    _module_under_test.wait_for_workers_idle();

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
}

TEST_F(PthreadWorkerPoolTest, TestSetPriority)
{
    constexpr int TEST_SCHED_PRIO_0 = 66;
    constexpr int TEST_SCHED_PRIO_1 = 77;
    auto res = _module_under_test.add_worker(worker_function, nullptr, TEST_SCHED_PRIO_0);
    ASSERT_EQ(WorkerPoolStatus::OK, res);
    res = _module_under_test.add_worker(worker_function, nullptr, TEST_SCHED_PRIO_1);
    ASSERT_EQ(WorkerPoolStatus::OK, res);

    pthread_attr_t task_attributes;
    struct sched_param rt_params;
    pthread_t worker_tid = _module_under_test._workers[0]->_thread_handle;
    auto pres = pthread_getattr_np(worker_tid, &task_attributes);
    ASSERT_EQ(pres, 0);
    pres = pthread_attr_getschedparam(&task_attributes, &rt_params);
    ASSERT_EQ(pres, 0);
    ASSERT_EQ(rt_params.sched_priority, TEST_SCHED_PRIO_0);

    worker_tid = _module_under_test._workers[1]->_thread_handle;
    pres = pthread_getattr_np(worker_tid, &task_attributes);
    ASSERT_EQ(pres, 0);
    pres = pthread_attr_getschedparam(&task_attributes, &rt_params);
    ASSERT_EQ(pres, 0);
    ASSERT_EQ(rt_params.sched_priority, TEST_SCHED_PRIO_1);
}

TEST_F(PthreadWorkerPoolTest, TestWrongPriority)
{
    auto res = _module_under_test.add_worker(worker_function, nullptr, -17);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res);
    res = _module_under_test.add_worker(worker_function, nullptr, 102);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res);
}


TEST_F(PthreadWorkerPoolTest, TestAutomaticAffinity)
{
    for (int i=0; i<N_TEST_WORKERS; i++)
    {
        auto res = _module_under_test.add_worker(worker_function, nullptr);
        ASSERT_EQ(WorkerPoolStatus::OK, res);
    }

    for (int i=0; i<N_TEST_WORKERS; i++)
    {
        pthread_attr_t task_attributes;
        pthread_t worker_tid = _module_under_test._workers[i]->_thread_handle;
        auto pres = pthread_getattr_np(worker_tid, &task_attributes);
        ASSERT_EQ(pres, 0);

        cpu_set_t cpus;
        pres = pthread_attr_getaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);
        ASSERT_EQ(pres, 0);
        for (int k=0; k<N_TEST_WORKERS; k++)
        {
            if (k == i)
            {
                ASSERT_TRUE(CPU_ISSET(k, &cpus));
            }
            else
            {
                ASSERT_FALSE(CPU_ISSET(k, &cpus));
            }
        }
    }

}


TEST_F(PthreadWorkerPoolTest, TestManualAffinity)
{
    int TEST_AFFINITIES[N_TEST_WORKERS] = {3, 2, 1, 1};
    for (int i=0; i<N_TEST_WORKERS; i++)
    {
        auto res = _module_under_test.add_worker(worker_function, nullptr, 75, TEST_AFFINITIES[i]);
        ASSERT_EQ(WorkerPoolStatus::OK, res);
    }

    for (int i=0; i<N_TEST_WORKERS; i++)
    {
        pthread_attr_t task_attributes;
        pthread_t worker_tid = _module_under_test._workers[i]->_thread_handle;
        auto pres = pthread_getattr_np(worker_tid, &task_attributes);
        ASSERT_EQ(pres, 0);

        cpu_set_t cpus;
        pres = pthread_attr_getaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);
        ASSERT_EQ(pres, 0);
        int expected_affinity = TEST_AFFINITIES[i];
        for (int k=0; k<N_TEST_WORKERS; k++)
        {
            if (k == expected_affinity)
            {
                ASSERT_TRUE(CPU_ISSET(k, &cpus));
            }
            else
            {
                ASSERT_FALSE(CPU_ISSET(k, &cpus));
            }
        }
    }

}

