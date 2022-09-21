# TWINE
## Thread and Worker INterface for Elk Audio OS

Support library for managing realtime threads and worker pools

Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk, Stockholm, Sweden.

## Usage
To include twine in a CMake based project the lines below needs to be added to the projects CMake configuration. Further build options can be found in twine/CMakeLists.txt
`add_subdirectory(twine)`
`target_link_libraries(project PRIVATE twine)`


This will also export the neccesary include folders so that the twine header file can then be included into source files with:

`#include "twine/twine.h`

By default, the target `twine` is a shared library. A static library target is also generated and can be linked instead by using `twine_objlib`.

## License

TWINE is licensed under the GNU General Public License v3 (“GPLv3”). Commercial licenses are available on request at tech@elk.audio.

Copyright 2017-2022 Modern Ancient Instruments Networked AB, dba Elk, Stockholm, Sweden.

