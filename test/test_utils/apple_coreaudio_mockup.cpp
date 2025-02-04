#include "elk-warning-suppressor/warning_suppressor.hpp"

ELK_PUSH_WARNING
ELK_DISABLE_NULLABILITY_COMPLETENESS
#include "apple_coreaudio_mockup.h"
ELK_POP_WARNING

AppleAudioHardwareMockup* AppleAudioHardwareMockup::instance = nullptr;

OSStatus AudioDeviceCreateIOProcID(AudioObjectID audio_object_id,
                                   AudioDeviceIOProc io_proc,
                                   void* client_data,
                                   AudioDeviceIOProcID* proc_id)
{
    return AppleAudioHardwareMockup::instance->AudioDeviceCreateIOProcID(
            audio_object_id,
            io_proc,
            client_data,
            proc_id);
}

OSStatus AudioDeviceDestroyIOProcID(AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id)
{
    return AppleAudioHardwareMockup::instance->AudioDeviceDestroyIOProcID(audio_object_id, proc_id);
}

OSStatus AudioDeviceStart(AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id)
{
    return AppleAudioHardwareMockup::instance->AudioDeviceStart(audio_object_id, proc_id);
}

OSStatus AudioDeviceStop(AudioObjectID audio_object_id, AudioDeviceIOProcID proc_id)
{
    return AppleAudioHardwareMockup::instance->AudioDeviceStop(audio_object_id, proc_id);
}

OSStatus AudioObjectGetPropertyData(AudioObjectID audio_object_id,
                                    const AudioObjectPropertyAddress* address,
                                    UInt32 qualifier_data_size,
                                    const void* __nullable qualifier_data,
                                    UInt32* data_size,
                                    void* out_data)
{
    return AppleAudioHardwareMockup::instance->AudioObjectGetPropertyData(
            audio_object_id,
            address,
            qualifier_data_size,
            qualifier_data,
            data_size,
            out_data);
}

OSStatus AudioObjectSetPropertyData(AudioObjectID audio_object_id,
                                    const AudioObjectPropertyAddress* address,
                                    UInt32 qualifier_data_size,
                                    const void* __nullable qualifier_data,
                                    UInt32 data_size,
                                    const void* data)
{
    return AppleAudioHardwareMockup::instance->AudioObjectSetPropertyData(
            audio_object_id,
            address,
            qualifier_data_size,
            qualifier_data,
            data_size,
            data);
}

OSStatus AudioObjectGetPropertyDataSize(AudioObjectID audio_object_id,
                                        const AudioObjectPropertyAddress* address,
                                        UInt32 qualifier_data_size,
                                        const void* __nullable qualifier_data,
                                        UInt32* out_data_size)
{
    return AppleAudioHardwareMockup::instance->AudioObjectGetPropertyDataSize(
            audio_object_id,
            address,
            qualifier_data_size,
            qualifier_data,
            out_data_size);
}

Boolean AudioObjectHasProperty(AudioObjectID audio_object_id, const AudioObjectPropertyAddress* address)
{
    return AppleAudioHardwareMockup::instance->AudioObjectHasProperty(audio_object_id, address);
}

OSStatus AudioObjectIsPropertySettable(AudioObjectID audio_object_id,
                                       const AudioObjectPropertyAddress* address,
                                       Boolean* out_is_settable)
{
    return AppleAudioHardwareMockup::instance->AudioObjectIsPropertySettable(audio_object_id, address, out_is_settable);
}

OSStatus AudioObjectAddPropertyListener(AudioObjectID audio_object_id,
                                        const AudioObjectPropertyAddress* address,
                                        AudioObjectPropertyListenerProc listener,
                                        void* __nullable client_data)
{
    return AppleAudioHardwareMockup::instance->AudioObjectAddPropertyListener(audio_object_id, address, listener, client_data);
}

OSStatus AudioObjectRemovePropertyListener(AudioObjectID audio_object_id,
                                           const AudioObjectPropertyAddress* address,
                                           AudioObjectPropertyListenerProc listener,
                                           void* __nullable client_data)
{
    return AppleAudioHardwareMockup::instance->AudioObjectRemovePropertyListener(audio_object_id, address, listener, client_data);
}

// These aren't strictly CoreAudio API methods, but for the sake of simplicity they are included here.
// All are used in conjunction with the CoreAudio threading methods.

int os_workgroup_join(os_workgroup_t wg, os_workgroup_join_token_t token_out)
{
    return AppleAudioHardwareMockup::instance->os_workgroup_join(wg, token_out);
}

bool os_workgroup_testcancel(os_workgroup_t wg)
{
    return AppleAudioHardwareMockup::instance->os_workgroup_testcancel(wg);
}

mach_port_t pthread_mach_thread_np(pthread_t thread)
{
    return AppleAudioHardwareMockup::instance->pthread_mach_thread_np(thread);
}
