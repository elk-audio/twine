# TWINE
## Thread and Worker INterface for Elk Audio OS

Support library for managing realtime threads and worker pools

Copyright Copyright 2017-2023 Elk Audio AB, Stockholm, Sweden.

## Usage
To include twine in a CMake based project the lines below needs to be added to the projects CMake configuration. Further build options can be found in twine/CMakeLists.txt
`add_subdirectory(twine)`
`target_link_libraries(project PRIVATE twine)`


This will also export the necessary include folders so that the twine header file can then be included into source files with:

`#include "twine/twine.h`

By default, the target `twine` is a shared library. A static library target is also generated and can be linked instead by using `twine_objlib`.

## CMake build options
| Option                              | Value    | Notes                                                                                                             |
|-------------------------------------|----------|-------------------------------------------------------------------------------------------------------------------|
| TWINE_WITH_XENOMAI                  | on / off | Build with Xenomai realtime thread support                                                                        |
| TWINE_WITH_TESTS                    | on / off | Build and run unit tests                                                                                          |
| TWINE_BUILD_WITH_APPLE_COREAUDIO    | on / off | Build with CoreAudio support on macOS. This is needed to support apple silicon real-time thread workgroups        |

On macOS, Apple CoreAudio is on by default - switching it off will significantly affect performance on Apple Silicon, since CoreAudio is needed for joining thread workgroups. 

## License

TWINE is licensed under the GNU General Public License v3 (“GPLv3”). Commercial licenses are available on request at tech@elk.audio.

Copyright Copyright 2017-2023 Elk Audio AB, Stockholm, Sweden.

