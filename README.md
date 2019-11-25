# TWINE
## Thread and Worker INterface for Elk music os

Support library for managing realtime threads and worker pools

Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm, Sweden.

## Usage
To include twine in a CMake based project the lines below needs to be added to the projects CMake configuration. Further build options can be found in twine/CMakeLists.txt
`add_subdirectory(twine)`
`target_link_libraries(project PRIVATE twine)`

This will also export the neccesary include folders so that the twine header file can then be included into source files with:

`#include "twine/twine.h`