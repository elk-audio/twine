## Next
New Features:
* Limited Windows support
* C++20 support

Breaking Changes:
* Renamed WorkerPoolStatus::ERROR to WorkerPoolStatus::POOL_ERROR

## 0.4.0
New Features:
* Full EVL support including isolated cpus
* Support for MacOS thread workgroups and real-time settings
* Posix version of RtConditionVariable uses semaphores for improved performance
* CMake install package

Fixes:
* Fix for UTF8 device name on MacOS
* Randomised suffix on named semaphores to avoid conflicts

## 0.3.2
New Features:
* Option for building a static library

## 0.3.1
New Features:
  * Worker priority and affinity is now settable per worker

Fixes:
  * Use named semaphores under Posix and macOS

## 0.3.0
New Features:
  * Thread pool uses semaphores for improved performance
  * Option to enable mode switch debugging on workers

Fixes:
  * Memory leak in thread attributes

## 0.2.1

Fixes:
  * CMake include folders when used as a library

## 0.2.0

New Features:
  * RtToNonRtConditionVariable for signaling non-rt threads from an rt thread.

Fixes:
  * Better clock synchronization between rt and non-rt domains

## 0.1.0

  * Initial version
  * Thread pool implementation
  * Utility functions
