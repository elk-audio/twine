#ifndef TWINE_APPLE_MOCK_LAMBDAS_H
#define TWINE_APPLE_MOCK_LAMBDAS_H

#include "twine/twine.h"

#include "../test_utils/apple_coreaudio_mockup.h"

#ifdef __APPLE__

#define MOCK_APPLE_THREADING

// Due to the order of file includes, mocking this with gMock didn't work.
// Since testing the setting of policy success isn't crucial, it's mocked here returning Success for every invocation.
kern_return_t thread_policy_set(mach_port_t, int, thread_policy_t, mach_msg_type_number_t)
{
    return KERN_SUCCESS;
}

// This cannot be mocked with Google mock as it may be called from a thread after the
// Google mock fixture has already been closed and de-allocated.
void os_workgroup_leave(os_workgroup_t, os_workgroup_join_token_t)
{

}

#endif

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO

using ::testing::NiceMock;
using ::testing::Return;

struct AppleTestData
{
    twine::apple::AppleMultiThreadData apple_data {"AudioDeviceName", 64, 48000};

    const char device_name[16] {"AudioDeviceName"};

    AudioDeviceID device_id {1};

    AudioDeviceID device_ids[2] {device_id, device_id};

    // This will be invalid if used - but if the pointer is just checked to not be null it's ok.
    os_workgroup_t _Nonnull workgroup {reinterpret_cast<os_workgroup_t>(device_id)};
};

typedef std::function<int(AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*)> MockSizeFunction;

typedef std::function<int(AudioObjectID, const AudioObjectPropertyAddress*, UInt32, const void*, UInt32*, void*)> MockDataFunction;

/**
 * This struct contains the lambdas used further down in gMock EXPECT WillOnce/WillRepeatedly calls.
 * That use is also why they are not member functions.
 * Declaring them directly as lambdas, while inelegant, is cleaner and more explicit than
 * std::bind calls with argument binding.
 */
struct MockLambdas
{
    AppleTestData& test_data;

    MockSizeFunction mock_size_of_one_device;
    MockSizeFunction mock_size_of_name;
    MockSizeFunction mock_size_of_workgroup;

    MockSizeFunction mock_device_size_failure;
    MockSizeFunction mock_workgroup_size_failure;

    MockDataFunction mock_devices_data_structure;
    MockDataFunction mock_device_name;
    MockDataFunction mock_workgroup;

    MockDataFunction mock_data_structure_failure;
    MockDataFunction mock_name_failure;
    MockDataFunction mock_name_mismatch_failure;
    MockDataFunction mock_workgroup_failure;

    explicit MockLambdas(AppleTestData& test_data_) : test_data(test_data_)
    {
        // Mocking size fetching:

        mock_size_of_one_device = [&](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size)
        {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            *out_data_size = 4;

            return kAudioHardwareNoError;
        };

        mock_size_of_name = [&](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size)
        {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioObjectPropertyName);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            *out_data_size = sizeof(CFStringRef);

            return kAudioHardwareNoError;
        };

        mock_size_of_workgroup = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* out_data_size)
        {
            // The mocked test doesn't create a workgroup instance to compare against here.
            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            *out_data_size = static_cast<int>(sizeof(test_data.workgroup));

            return kAudioHardwareNoError;
        };

        // Mocking size fetch failures:

        mock_device_size_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*)
        {
            EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
            return 10;
        };

        mock_workgroup_size_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*) {
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
            return 10;
        };

        // Mocking data fetching:

        mock_devices_data_structure = [&](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data)
        {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            EXPECT_EQ(sizeof(out_data), sizeof(test_data.device_ids));
            memcpy(out_data, test_data.device_ids, sizeof(test_data.device_ids));
            return noErr;
        };

        mock_device_name = [&](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data)
        {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioObjectPropertyName);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            auto size_of_ref = sizeof(CFStringRef);

            // This method mocks an apple-API method, that allocates memory,
            // which the invoker then has to release with CFRelease.
            // We do that in the production code e.g. get_device_workgroup,
            // meaning, if the test leaks memory, that is an issue with the production code - not this test.
            auto cfStr = CFStringCreateWithCString(kCFAllocatorDefault,
                                                   test_data.device_name,
                                                   kCFStringEncodingUTF8);

            memcpy(out_data, &cfStr, size_of_ref);

            *size = size_of_ref;

            return noErr;
        };

        mock_workgroup = [&](AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data)
        {
            EXPECT_EQ(audio_object_id, kAudioObjectSystemObject);

            EXPECT_NE(address, nullptr);
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
            EXPECT_EQ(address->mScope, kAudioObjectPropertyScopeGlobal);
            EXPECT_EQ(address->mElement, kAudioObjectPropertyElementMain);

            EXPECT_EQ(sizeof(out_data), sizeof(test_data.workgroup));
            return noErr;
        };

        // Mocking data structure failures

        mock_data_structure_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void*)
        {
            EXPECT_EQ(address->mSelector, kAudioHardwarePropertyDevices);
            return 10;
        };

        mock_name_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void*)
        {
            EXPECT_EQ(address->mSelector, kAudioObjectPropertyName);
            return 10;
        };

        mock_name_mismatch_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32* size, void* out_data)
        {
            EXPECT_EQ(address->mSelector, kAudioObjectPropertyName);

            auto size_of_ref = sizeof(CFStringRef);

            // This method mocks an apple-API method, that allocates memory,
            // which the invoker then has to release with CFRelease.
            // We do that in the production code e.g. get_device_workgroup,
            // meaning, if the test leaks memory, that is an issue with the production code - not this test.
            auto cfStr = CFStringCreateWithCString(kCFAllocatorDefault,
                                                   test_data.device_name,
                                                   kCFStringEncodingUTF8);

            memcpy(out_data, &cfStr, size_of_ref);

            *size = size_of_ref;

            return noErr;
        };

        mock_workgroup_failure = [&](AudioObjectID, const AudioObjectPropertyAddress* address, UInt32, const void*, UInt32*, void* out_data)
        {
            EXPECT_EQ(address->mSelector, kAudioDevicePropertyIOThreadOSWorkgroup);
            EXPECT_EQ(sizeof(out_data), sizeof(test_data.workgroup));
            return 10;
        };
    }
};

void workgroup_success_expectations(testing::StrictMock<AppleAudioHardwareMockup>& mock, MockLambdas& mock_lambdas)
{
    EXPECT_CALL(mock, AudioObjectGetPropertyDataSize)
            .WillOnce(mock_lambdas.mock_size_of_one_device) // Getting the size of devices - pretending there's one device:
            .WillOnce(mock_lambdas.mock_size_of_name) // Getting the size of the name:
            .WillOnce(mock_lambdas.mock_size_of_workgroup); // Getting the size of workgroup:

    EXPECT_CALL(mock, AudioObjectGetPropertyData)
            .WillOnce(mock_lambdas.mock_devices_data_structure) // Getting the devices data structure
            .WillOnce(mock_lambdas.mock_device_name) // Getting the device name
            .WillOnce(mock_lambdas.mock_workgroup); // Getting the workgroup

    EXPECT_CALL(mock, os_workgroup_testcancel).WillRepeatedly(Return(false));
}

void workgroup_repeated_success_expectations(testing::StrictMock<AppleAudioHardwareMockup>& mock, MockLambdas mock_lambdas)
{
    EXPECT_CALL(mock, AudioObjectGetPropertyDataSize)
            .WillOnce(mock_lambdas.mock_size_of_one_device) // Getting the size of devices - pretending there's one device
            .WillOnce(mock_lambdas.mock_size_of_name) // Getting the size of the name
            .WillOnce(mock_lambdas.mock_size_of_workgroup) // Getting the size of workgroup
            .WillOnce(mock_lambdas.mock_size_of_one_device) // Getting the size of devices - pretending there's one device
            .WillOnce(mock_lambdas.mock_size_of_name) // Getting the size of the name
            .WillOnce(mock_lambdas.mock_size_of_workgroup); // Getting the size of workgroup

    EXPECT_CALL(mock, AudioObjectGetPropertyData)
            .WillOnce(mock_lambdas.mock_devices_data_structure) // Getting the devices data structure
            .WillOnce(mock_lambdas.mock_device_name) // Getting the device name
            .WillOnce(mock_lambdas.mock_workgroup) // Getting the workgroup
            .WillOnce(mock_lambdas.mock_devices_data_structure) // Getting the devices data structure
            .WillOnce(mock_lambdas.mock_device_name) // Getting the device name
            .WillOnce(mock_lambdas.mock_workgroup); // Getting the workgroup

    EXPECT_CALL(mock, os_workgroup_testcancel).WillRepeatedly(Return(false));
}

void workgroup_failure_expectations(testing::StrictMock<AppleAudioHardwareMockup>& mock, MockLambdas& mock_lambdas)
{
    EXPECT_CALL(mock, AudioObjectGetPropertyDataSize)
            .WillOnce(mock_lambdas.mock_size_of_one_device) // Getting the size of devices - pretending there's one device:
            .WillOnce(mock_lambdas.mock_size_of_name) // Getting the size of the name:
            .WillOnce(mock_lambdas.mock_size_of_workgroup); // Getting the size of workgroup:

    EXPECT_CALL(mock, AudioObjectGetPropertyData)
            .WillOnce(mock_lambdas.mock_devices_data_structure) // Getting the devices data structure
            .WillOnce(mock_lambdas.mock_device_name) // Getting the device name
            .WillOnce(mock_lambdas.mock_workgroup_failure); // Getting the workgroup

    EXPECT_CALL(mock, os_workgroup_testcancel).WillRepeatedly(Return(false));
}

#else
struct AppleTestData
{
    twine::apple::AppleMultiThreadData apple_data {"AudioDeviceName", 64, 48000};
};
#endif

#endif //TWINE_APPLE_MOCK_LAMBDAS_H
