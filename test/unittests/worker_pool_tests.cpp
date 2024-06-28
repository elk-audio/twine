
#include <thread>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elk-warning-suppressor/warning_suppressor.hpp"

ELK_PUSH_WARNING

ELK_DISABLE_KEYWORD_MACRO
#define private public

ELK_DISABLE_NULLABILITY_COMPLETENESS

#include "../test_utils/apple_mock_lambdas.h"

#include "apple_threading.cpp"

#include "worker_pool_implementation.h"

using ::testing::NiceMock;
using ::testing::Return;

using namespace twine;

namespace
{
    constexpr int N_TEST_WORKERS = 4;
    constexpr int TEST_AUDIO_CHUNK_SIZE = 64;
    constexpr int TEST_SAMPLE_RATE = 48000;
}

void test_function(std::atomic_bool& running, std::atomic_bool& flag, BarrierWithTrigger<ThreadType::PTHREAD>(& barrier))
{
    while (running)
    {
        barrier.wait();
        flag = true;
    }
}

TEST (UtilityFunctionTest, TestGetIsolatedCpus)
{
    auto res = read_isolated_cores("0-3");
    ASSERT_EQ(4, res.size());
    EXPECT_EQ(0, res.at(0));
    EXPECT_EQ(1, res.at(1));
    EXPECT_EQ(2, res.at(2));
    EXPECT_EQ(3, res.at(3));

    res = read_isolated_cores("2-3");
    ASSERT_EQ(2, res.size());
    EXPECT_EQ(2, res.at(0));
    EXPECT_EQ(3, res.at(1));

    res = read_isolated_cores("23");
    EXPECT_TRUE(res.empty());

    res = read_isolated_cores("");
    EXPECT_TRUE(res.empty());

    res = read_isolated_cores("4-");
    EXPECT_TRUE(res.empty());
}

TEST (UtilityFunctionTest, TestBuildCoreList)
{
    auto list = build_core_list(2, 3);
    ASSERT_EQ(3, list.size());
    EXPECT_EQ(2, list.at(0).id);
    EXPECT_EQ(3, list.at(1).id);
    EXPECT_EQ(4, list.at(2).id);
}

TEST (BarrierTest, TestBarrierWithTrigger)
{
    std::atomic_bool a = false;
    std::atomic_bool b = false;
    std::atomic_bool running = true;

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

    /* Do it again with the single function release and wait */
    a = false;
    b = false;
    module_under_test.release_and_wait();
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
    void SetUp() override
    {
#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
        AppleAudioHardwareMockup::instance = &_mock;
#endif
    }

    void TearDown() override
    {
    }

    AppleTestData _test_data;

    WorkerPoolImpl<ThreadType::PTHREAD> _module_under_test {N_TEST_WORKERS,
                                                            _test_data.apple_data,
                                                            true,
                                                            false};

    bool a {false};
    bool b {false};

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
    testing::StrictMock<AppleAudioHardwareMockup> _mock;
#endif
};

void worker_function(void* data)
{
    bool* flag = reinterpret_cast<bool*>(data);
    *flag = true;
}

TEST_F(PthreadWorkerPoolTest, FunctionalityTest)
{
#ifdef TWINE_APPLE_THREADING
    _module_under_test._apple_data.chunk_size = TEST_AUDIO_CHUNK_SIZE;
    _module_under_test._apple_data.current_sample_rate = TEST_SAMPLE_RATE;

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
    MockLambdas mock_lambdas(_test_data);
    workgroup_repeated_success_expectations(_mock, mock_lambdas);

    EXPECT_CALL(_mock, os_workgroup_join).WillRepeatedly(Return(0)); // 0 for success
    EXPECT_CALL(_mock, pthread_mach_thread_np).WillRepeatedly(Return(true));
#endif

#endif

    auto res = _module_under_test.add_worker(worker_function, &a);
    ASSERT_EQ(WorkerPoolStatus::OK, res.first);

    res = _module_under_test.add_worker(worker_function, &b);
    ASSERT_EQ(WorkerPoolStatus::OK, res.first);

    ASSERT_FALSE(a);
    ASSERT_FALSE(b);

    _module_under_test.wakeup_workers();
    _module_under_test.wait_for_workers_idle();

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
}

#ifndef __APPLE__
TEST_F(PthreadWorkerPoolTest, TestSetPriority)
{
    constexpr int TEST_SCHED_PRIO_0 = 66;
    constexpr int TEST_SCHED_PRIO_1 = 77;
    auto res = _module_under_test.add_worker(worker_function, nullptr, TEST_SCHED_PRIO_0);
    ASSERT_EQ(WorkerPoolStatus::OK, res.first);
    res = _module_under_test.add_worker(worker_function, nullptr, TEST_SCHED_PRIO_1);
    ASSERT_EQ(WorkerPoolStatus::OK, res.first);

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
#endif

TEST_F(PthreadWorkerPoolTest, TestWrongPriority)
{
#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
    MockLambdas mock_lambdas(_test_data);
    workgroup_repeated_success_expectations(_mock, mock_lambdas);
#endif

    auto res = _module_under_test.add_worker(worker_function, nullptr, -17);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res.first);

    res = _module_under_test.add_worker(worker_function, nullptr, 102);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res.first);
}

#ifndef __APPLE__
TEST_F(PthreadWorkerPoolTest, TestAutomaticAffinity)
{
    for (int i=0; i<N_TEST_WORKERS; i++)
    {
        auto res = _module_under_test.add_worker(worker_function, nullptr);
        ASSERT_EQ(WorkerPoolStatus::OK, res.first);
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
        ASSERT_EQ(WorkerPoolStatus::OK, res.first);
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
#endif

TEST_F(PthreadWorkerPoolTest, TestManualAffinityOutOfRange)
{
    auto res = _module_under_test.add_worker(worker_function, nullptr, 75, N_TEST_WORKERS+1);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res.first);
}

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO

class AppleThreadingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        AppleAudioHardwareMockup::instance = &_mock;
    }

    void TearDown() override
    {
    }

    AppleTestData _test_data;

    testing::StrictMock<AppleAudioHardwareMockup> _mock;
};

TEST_F(AppleThreadingTest, GetDeviceWorkgroupSuccess)
{
    MockLambdas mock_lambdas(_test_data);

    workgroup_success_expectations(_mock, mock_lambdas);

    auto device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);

    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::OK);
}

TEST_F(AppleThreadingTest, GetDeviceWorkgroupFailures)
{
    MockLambdas mock_lambdas(_test_data);

    EXPECT_CALL(_mock, AudioObjectGetPropertyDataSize)

    // First invocation:
    // Pretending failure in getting the size of devices.
    .WillOnce(mock_lambdas.mock_device_size_failure)

    // Second Invocation:
    // Subsequent invocation can go well, so that devices size can fail.
    .WillOnce(Return(noErr))
    // Getting the size of devices, so that getting name can fail
    .WillOnce(mock_lambdas.mock_size_of_two_devices)
    // Getting the size of the name
    .WillOnce(mock_lambdas.mock_size_of_name)

    // Third invocation:
    // Getting the size of devices, so that getting workgroup size can fail.
    .WillOnce(mock_lambdas.mock_size_of_two_devices)
    // Getting the size of the name.
    .WillOnce(mock_lambdas.mock_size_of_name)
    // Pretending failure in getting the size of workgroup.
    .WillOnce(mock_lambdas.mock_workgroup_size_failure)

    // Fourth Invocation:
    // Getting the size of devices
    .WillOnce(mock_lambdas.mock_size_of_two_devices)
    // Getting the size of the name.
    .WillOnce(mock_lambdas.mock_size_of_name)
    // Getting the size of workgroup can succeed, so that getting workgroup can fail.
    .WillOnce(Return(noErr))

    // Fifth Invocation:
    // Getting the size of devices.
    .WillOnce(mock_lambdas.mock_size_of_two_devices)
    // Getting the size of the name.
    .WillOnce(mock_lambdas.mock_size_of_name)
    // Getting the size of workgroup can succeed, so that workgroup cancellation status can fail.
    .WillOnce(Return(noErr))

    // Sixth Invocation:
    // Getting the size of devices.
    .WillOnce(mock_lambdas.mock_size_of_two_devices)
    // Getting the size of the name.
    .WillOnce(mock_lambdas.mock_size_of_name)
    .WillOnce(mock_lambdas.mock_size_of_name);

    EXPECT_CALL(_mock, AudioObjectGetPropertyData)

    // Second invocation:
    // Pretending failure in getting the devices data structure.
    .WillOnce(mock_lambdas.mock_data_structure_failure)

    // Third invocation:
    // Allowing getting data structure to succeed...
    .WillOnce(mock_lambdas.mock_devices_data_structure)
    // So that getting the device name can fail.
    .WillOnce(mock_lambdas.mock_name_failure)

    // Fourth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce(mock_lambdas.mock_devices_data_structure)
    // As can getting the device name.
    .WillOnce(mock_lambdas.mock_device_name)

    // Fifth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce(mock_lambdas.mock_devices_data_structure)
    // As can getting the device name.
    .WillOnce(mock_lambdas.mock_device_name)
    // So that getting workgroup can fail.
    .WillOnce(mock_lambdas.mock_workgroup_failure)

    // Sixth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce(mock_lambdas.mock_devices_data_structure)
    // As can getting the device name.
    .WillOnce(mock_lambdas.mock_device_name)
    // And getting workgroup.
    .WillOnce(Return(noErr))

    // Seventh invocation:
    // Allowing getting data structure to succeed...
    .WillOnce(mock_lambdas.mock_devices_data_structure)
    // And getting the device name succeeds, BUT RETURNS ONE THAT WON'T MATCH.
    .WillOnce(mock_lambdas.mock_device_name)
    .WillOnce(mock_lambdas.mock_name_mismatch_failure);

    // Letting the fetching of workgroup cancellation status fail (sixth)
    EXPECT_CALL(_mock, os_workgroup_testcancel).WillOnce(Return(true));

    auto device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::PD_SIZE_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::PD_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::FETCH_NAME_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_SIZE_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_CANCELLED);

    _test_data.apple_data.device_name = "ThisWillNotMatch";
    device_workgroup_result = twine::apple::get_device_workgroup(_test_data.apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::INVALID_DEVICE_NAME_PASSED);
}

TEST_F(AppleThreadingTest, TestThreadWorkgroupJoinSuccess)
{
    EXPECT_CALL(_mock, os_workgroup_join).WillOnce(Return(0)); // 0 for success
    EXPECT_CALL(_mock, os_workgroup_testcancel).WillOnce(Return(false));

    auto result = twine::apple::join_workgroup(_test_data.workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::OK);
}

TEST_F(AppleThreadingTest, TestThreadWorkgroupJoinFailure)
{
    EXPECT_CALL(_mock, os_workgroup_testcancel)

    // First execution, testing cancellation:
    .WillOnce(Return(true))

    // Second execution, testing already cancelled
    .WillOnce(Return(false))

    // Third execution, testing already cancelled
    .WillOnce(Return(false));

    EXPECT_CALL(_mock, os_workgroup_join)

    // Second execution, testing already cancelled when attempting to join
    .WillOnce(Return(EINVAL))

    // Second execution, testing unknown WG failure
    .WillOnce(Return(10));

    auto result = twine::apple::join_workgroup(_test_data.workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED);

    result = twine::apple::join_workgroup(_test_data.workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED);

    result = twine::apple::join_workgroup(_test_data.workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_JOINING_UNKNOWN_FAILURE);
}

TEST_F(AppleThreadingTest, FunctionalityFailureTest)
{
#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
    MockLambdas mock_lambdas(_test_data);
    workgroup_failure_expectations(_mock, mock_lambdas);
#endif

    EXPECT_CALL(_mock, pthread_mach_thread_np).WillRepeatedly(Return(true));

    WorkerPoolImpl<ThreadType::PTHREAD> worker_pool {N_TEST_WORKERS,
                                                     _test_data.apple_data,
                                                     true,
                                                     false};

    bool a {false};

    auto res = worker_pool.add_worker(worker_function, &a);

    EXPECT_EQ(res.first, WorkerPoolStatus::ERROR);
    EXPECT_EQ(res.second, apple::AppleThreadingStatus::NO_WORKGROUP_PASSED);
}


#endif

ELK_POP_WARNING