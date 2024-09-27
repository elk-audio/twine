# TWINE
## Thread and Worker INterface for Elk Audio OS

Support library for managing realtime threads and worker pools

Copyright Copyright 2017-2024 Elk Audio AB, Stockholm, Sweden.

## Description
Twine contains a number of utility classes and functions and was designed to abstract away the details of working with realtime Xenomai/EVL threads and posix threads. As such, most of the classes and functions have dual implementations that are selected at runtime depending on whether they run in a Xenomai/EVL enabled host or not..

Twine can also be build with only Posix, macOS or Windows support to enable development of a plugins and applications for Elk Audio OS on a non Elk enabled platform.


## Usage
To include twine in a CMake based project the lines below need to be added to the project's CMake configuration. Further build options can be found in twine/CMakeLists.txt
`add_subdirectory(twine)`

`target_link_libraries(project PRIVATE twine)`


This will also export the necessary include folders so the twine header file can be included in source files with:

`#include "twine/twine.h`

By default, the target `twine` is a shared library. A static library target is also generated and can be linked instead by using `twine_static`.

## CMake build options
| Option                           | Value    | Notes                                                                                                      |
|----------------------------------|----------|------------------------------------------------------------------------------------------------------------|
| TWINE_WITH_XENOMAI               | on / off | Build with Xenomai 3 realtime thread support. Mutually exclusive with TWINE_WITH_EVL.                      |
| TWINE_WITH_EVL                   | on / off | Build with EVL realtime thread support. Mutually exclusive with TWINE_WITH_XENOMAI.                        |
| TWINE_WITH_TESTS                 | on / off | Build and run unit tests                                                                                   |
| TWINE_BUILD_WITH_APPLE_COREAUDIO | on / off | Build with CoreAudio support on macOS. This is needed to support apple silicon real-time thread workgroups |

On macOS, Apple CoreAudio is on by default - switching it off will significantly affect performance on Apple Silicon, since CoreAudio is needed for joining thread workgroups. 

## License

TWINE is licensed under the GNU General Public License v3 (“GPLv3”). Commercial licenses are available on request at tech@elk.audio.

Copyright Copyright 2017-2024 Elk Audio AB, Stockholm, Sweden.

