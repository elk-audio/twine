## 0.3.3
New Features:
* On macOS, worker threads can now correctly use real-time settings, and thread workgroups on available platforms.

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
