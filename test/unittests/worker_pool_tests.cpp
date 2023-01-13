
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"// Ignore Apple nonsense
#include "../test_utils/apple_coreaudio_mockup.h"
#pragma clang diagnostic pop

#include <thread>
#include <functional>

#include "gtest/gtest.h"

#define private public
#ifdef __APPLE__
#include "apple_threading.cpp"

#endif
#include "worker_pool_implementation.h"

using ::testing::NiceMock;
using ::testing::Return;

using namespace twine;

constexpr int N_TEST_WORKERS = 4;

void test_function(std::atomic_bool& running, std::atomic_bool& flag, BarrierWithTrigger<ThreadType::PTHREAD>(& barrier))
{
    while (running)
    {
        barrier.wait();
        flag = true;
    }
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
        AppleAudioHardwareMockup::instance = &_mock;
    }

    void TearDown() override
    {
    }

    apple::AppleMultiThreadData apple_data {"AudioDeviceName", 64, 48000};
    apple::WorkerErrorCallback worker_error_cb {[]([[maybe_unused]] apple::AppleThreadingStatus){}};

    WorkerPoolImpl<ThreadType::PTHREAD> _module_under_test {N_TEST_WORKERS,
                                                            apple_data,
                                                            worker_error_cb,
                                                            true,
                                                            false};

    bool a{false};
    bool b{false};

    testing::StrictMock<AppleAudioHardwareMockup> _mock;
};

void worker_function(void* data)
{
    bool* flag = reinterpret_cast<bool*>(data);
    *flag = true;
}

TEST_F(PthreadWorkerPoolTest, FunctionalityTest)
{
    EXPECT_CALL(_mock, AudioObjectHasProperty).WillRepeatedly(Return(true));
    EXPECT_CALL(_mock, AudioObjectGetPropertyDataSize).WillRepeatedly(Return(noErr));
    EXPECT_CALL(_mock, AudioObjectGetPropertyData).WillRepeatedly(Return(noErr));

    EXPECT_CALL(_mock, pthread_mach_thread_np).WillRepeatedly(Return(true));

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

#ifndef __APPLE__
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
#endif

TEST_F(PthreadWorkerPoolTest, TestWrongPriority)
{
    EXPECT_CALL(_mock, AudioObjectHasProperty).WillRepeatedly(Return(true));
    EXPECT_CALL(_mock, AudioObjectGetPropertyDataSize).WillRepeatedly(Return(noErr));
    EXPECT_CALL(_mock, AudioObjectGetPropertyData).WillRepeatedly(Return(noErr));

    auto res = _module_under_test.add_worker(worker_function, nullptr, -17);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res);
    res = _module_under_test.add_worker(worker_function, nullptr, 102);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res);
}

#ifndef __APPLE__
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
#endif

TEST_F(PthreadWorkerPoolTest, TestManualAffinityOutOfRange)
{
    auto res = _module_under_test.add_worker(worker_function, nullptr, 75, N_TEST_WORKERS+1);
    ASSERT_EQ(WorkerPoolStatus::INVALID_ARGUMENTS, res);
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

    apple::AppleMultiThreadData apple_data;
    apple::WorkerErrorCallback worker_error_cb;

    AudioObjectPropertyAddress test_property_address;
    AudioDeviceID test_device_id {1};
    AudioDeviceID test_device_ids[2] {test_device_id, test_device_id};

    // This will be invalid if used - but if the pointer is just checked to not be null it's ok.
    os_workgroup_t _Nonnull test_workgroup {reinterpret_cast<os_workgroup_t>(test_device_id)};
    os_workgroup_join_token_s* test_join_token = nullptr;

    const char test_device_name[16] {"AudioDeviceName"};

    testing::StrictMock<AppleAudioHardwareMockup> _mock;
};

TEST_F(AppleThreadingTest, GetDeviceWorkgroupSuccess)
{
    apple_data.chunk_size = 64;
    apple_data.current_sample_rate = 48000;
    apple_data.device_name = "AudioDeviceName";

    EXPECT_CALL(_mock, AudioObjectGetPropertyDataSize)
            // Getting the size of devices - pretending there's one device:
            .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
                EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

                EXPECT_NE(address, nullptr);
                EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
                EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
                EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

                *out_data_size = 4;

                return kAudioHardwareNoError;
            })
            // Getting the size of the name:
            .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
                EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

                EXPECT_NE(address, nullptr);
                EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
                EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
                EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

                *out_data_size = sizeof(test_device_name);

                return kAudioHardwareNoError;
            })
            // Getting the size of workgroup:
            .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
                // The mocked test doesn't create a workgroup instance to compare against here.
                EXPECT_NE(address, nullptr);
                EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
                EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
                EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

                *out_data_size = static_cast<int>(sizeof(test_workgroup));

                return kAudioHardwareNoError;
            });

    EXPECT_CALL(_mock, AudioObjectGetPropertyData)
        // Getting the devices data structure:
        .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data) {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            EXPECT_EQ(sizeof(out_data), sizeof(test_device_ids));
            memcpy(out_data, test_device_ids, sizeof(test_device_ids));
            return noErr;
        })
        // Getting the device name:
        .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data) {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            strcpy((char*)out_data, test_device_name);

            *size = sizeof(test_device_name);

            return noErr;
        })
        // Getting the workgroup:
        .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data) {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeWildcard);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            EXPECT_EQ(sizeof(out_data), sizeof(test_workgroup));
            return noErr;
        });

    EXPECT_CALL(_mock, os_workgroup_testcancel).WillOnce(Return(false));

    auto device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);

    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::OK);
}

TEST_F(AppleThreadingTest, GetDeviceWorkgroupFailures)
{
    apple_data.chunk_size = 64;
    apple_data.current_sample_rate = 48000;
    apple_data.device_name = "AudioDeviceName";

    EXPECT_CALL(_mock, AudioObjectGetPropertyDataSize)

    // First invocation:
    // Pretending failure in getting the size of devices:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        return 10;
    })

    // Second Invocation:
    // Subsequent invocation can go well, so that devices size can fail.
    .WillOnce(Return(noErr))
    // Getting the size of devices - pretending there's one device, so that getting name can fail:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        *out_data_size = 4;
        return kAudioHardwareNoError;
    })
    // Getting the size of the name:
    .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        *out_data_size = sizeof(test_device_name);
        return kAudioHardwareNoError;
    })

    // Third invocation:
    // Getting the size of devices - pretending there's one device, so that getting workgroup size can fail:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        *out_data_size = 4;
        return kAudioHardwareNoError;
    })
    // Getting the size of the name:
    .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        *out_data_size = sizeof(test_device_name);
        return kAudioHardwareNoError;
    })
    // Pretending failure in getting the size of workgroup:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
        return 10;
    })

    // Fourth Invocation:
    // Getting the size of devices - pretending there's one device:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        *out_data_size = static_cast<int>(sizeof(AudioObjectPropertyAddress));
        return kAudioHardwareNoError;
    })
    // Getting the size of the name:
    .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        *out_data_size = sizeof(test_device_name);
        return kAudioHardwareNoError;
    })
    // Getting the size of workgroup can succeed, so that getting workgroup can fail:
    .WillOnce(Return(noErr))

    // Fifth Invocation:
    // Getting the size of devices - pretending there's one device:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        *out_data_size = static_cast<int>(sizeof(AudioObjectPropertyAddress));
        return kAudioHardwareNoError;
    })
    // Getting the size of the name:
    .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        *out_data_size = sizeof(test_device_name);
        return kAudioHardwareNoError;
    })
    // Getting the size of workgroup can succeed, so that workgroup cancellation status can fail:
    .WillOnce(Return(noErr))

    // Sixth Invocation:
    // Getting the size of devices - pretending there's one device:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        *out_data_size = 4;
        return kAudioHardwareNoError;
    })
    // Getting the size of the name:
    .WillOnce([=](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size) {
        EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        *out_data_size = sizeof(test_device_name);
        return kAudioHardwareNoError;
    });

    EXPECT_CALL(_mock, AudioObjectGetPropertyData)

    // Second invocation:
    // Pretending failure in getting the devices data structure:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void*) {
        EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
        return 10;
    })

    // Third invocation:
    // Allowing getting data structure to succeed...
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void* out_data) {
        memcpy(out_data, test_device_ids, sizeof(test_device_ids));
        return noErr;
    })
    // So that getting the device name can fail:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void*) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        return 10;
    })

    // Fourth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void* out_data) {
        memcpy(out_data, test_device_ids, sizeof(test_device_ids));
        return noErr;
    })
    // As can getting the device name:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        strcpy((char*)out_data, test_device_name);
        *size = sizeof(test_device_name);
        return noErr;
    })

    // Fifth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void* out_data) {
        memcpy(out_data, test_device_ids, sizeof(test_device_ids));
        return noErr;
    })
    // As can getting the device name:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        strcpy((char*)out_data, test_device_name);
        *size = sizeof(test_device_name);
        return noErr;
    })
    // So that getting workgroup can fail
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
        EXPECT_EQ(sizeof(out_data), sizeof(test_workgroup));
        return 10;
    })

    // Sixth invocation:
    // Allowing getting data structure to succeed...
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void* out_data) {
        memcpy(out_data, test_device_ids, sizeof(test_device_ids));
        return noErr;
    })
    // As can getting the device name:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        strcpy((char*)out_data, test_device_name);
        *size = sizeof(test_device_name);
        return noErr;
    })
    // And getting workgroup.
    .WillOnce(Return(noErr))

    // Seventh invocation:
    // Allowing getting data structure to succeed...
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void* out_data) {
        memcpy(out_data, test_device_ids, sizeof(test_device_ids));
        return noErr;
    })
    // And getting the device name succeeds, BUT RETURNS ONE THAT WON'T MATCH:
    .WillOnce([=](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data) {
        EXPECT_EQ(address->mSelector, kAudioDevicePropertyDeviceName);
        strcpy((char*)out_data, test_device_name);
        *size = sizeof(test_device_name);
        return noErr;
    });

    // Letting the fetching of workgroup cancellation status fail (sixth)
    EXPECT_CALL(_mock, os_workgroup_testcancel).WillOnce(Return(true));

    auto device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::PD_SIZE_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::PD_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::FETCH_NAME_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_SIZE_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_FAILED);

    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::WG_CANCELLED);

    apple_data.device_name = "ThisWillNotMatch";
    device_workgroup_result = twine::apple::get_device_workgroup(apple_data.device_name);
    EXPECT_EQ(device_workgroup_result.second, twine::apple::AppleThreadingStatus::INVALID_DEVICE_NAME_PASSED);
}

TEST_F(AppleThreadingTest, TestThreadWorkgroupJoinSuccess)
{
    EXPECT_CALL(_mock, os_workgroup_join).WillOnce(Return(0)); // 0 for success
    EXPECT_CALL(_mock, os_workgroup_testcancel).WillOnce(Return(false));

    auto result = twine::apple::join_workgroup(test_workgroup);
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

    auto result = twine::apple::join_workgroup(test_workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED);

    result = twine::apple::join_workgroup(test_workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED);

    result = twine::apple::join_workgroup(test_workgroup);
    EXPECT_EQ(result.first, twine::apple::AppleThreadingStatus::WORKGROUP_JOINING_UNKNOWN_FAILURE);
}


#endif
