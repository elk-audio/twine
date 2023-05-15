#ifndef SUSHI_APPLE_COREAUDIO_MOCKUP_H
#define SUSHI_APPLE_COREAUDIO_MOCKUP_H

#ifdef __APPLE__

#include <CoreAudio/AudioHardware.h>
#include <mach/mach_types.h>

#include <gmock/gmock.h>

class AppleAudioHardwareMockup
{
public:
    MOCK_METHOD(Boolean, AudioObjectHasProperty, (AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address));
    MOCK_METHOD(OSStatus, AudioObjectGetPropertyData, (AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32 qualifier_data_size, const void* qualifier_data, UInt32* data_size, void* data));
    MOCK_METHOD(OSStatus, AudioObjectSetPropertyData, (AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32 qualifier_data_size, const void* qualifier_data, UInt32 data_size, const void* data));
    MOCK_METHOD(OSStatus, AudioObjectGetPropertyDataSize, (AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, UInt32 qualifier_data_size, const void* qualifier_data, UInt32* out_data_size));
    MOCK_METHOD(OSStatus, AudioObjectIsPropertySettable, (AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address, Boolean* out_is_settable));

    MOCK_METHOD(OSStatus, AudioDeviceCreateIOProcID, (AudioObjectID audio_object_id, AudioDeviceIOProc io_proc, void* client_data, AudioDeviceIOProcID* io_proc_id));
    MOCK_METHOD(OSStatus, AudioDeviceDestroyIOProcID, (AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id));
    MOCK_METHOD(OSStatus, AudioDeviceStart, (AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id));
    MOCK_METHOD(OSStatus, AudioDeviceStop, (AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id));

    MOCK_METHOD(OSStatus, AudioObjectAddPropertyListener, (AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, AudioObjectPropertyListenerProc inListener, void* __nullable inClientData));
    MOCK_METHOD(OSStatus, AudioObjectRemovePropertyListener, (AudioObjectID inObjectID, const AudioObjectPropertyAddress* inAddress, AudioObjectPropertyListenerProc inListener, void* __nullable inClientData));

    MOCK_METHOD(bool, os_workgroup_testcancel, (os_workgroup_t wg));
    MOCK_METHOD(int, os_workgroup_join, (os_workgroup_t wg, os_workgroup_join_token_t token_out));

    MOCK_METHOD(mach_port_t, pthread_mach_thread_np, (pthread_t));

    static AppleAudioHardwareMockup* instance;
};

#endif

#endif// SUSHI_APPLE_COREAUDIO_MOCKUP_H
