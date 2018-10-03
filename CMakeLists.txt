cmake_minimum_required(VERSION 3.8)
project(twine)

########################
#  Build Options       #
########################

option(TWINE_WITH_XENOMAI "Build with xenomai realtime thread support" OFF)
option(TWINE_WITH_TESTS "Build and run unit tests" ON)

#######################
#  Cross compilation  #
#######################

if(NOT "$ENV{CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "$ENV{CMAKE_SYSROOT}")
endif()

if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(XENOMAI_BASE_DIR "${CMAKE_SYSROOT}/usr/xenomai")
    message("XENOMAI_BASE_DIR is " ${XENOMAI_BASE_DIR})
endif()

#######################
#  Utility functions  #
#######################

set(XENOMAI_BASE_DIR "/usr/xenomai" CACHE STRING "xenomai base dir path")

# Set Xenomai build options at local scope
# This intentionally does not include all the super-auto-magic linker wrappers,
# who seems to play very badly with other libraries at static initialization.
function(add_xenomai_to_target target)
    add_definitions(-DTWINE_BUILD_WITH_XENOMAI)

    # from `xeno-config --skin=posix --cflags`
    set(XENOMAI_C_FLAGS "-D_GNU_SOURCE -D_REENTRANT -D__COBALT__ -D__COBALT_WRAP__")

    set(XENOMAI_INCLUDE_DIRS
        ${XENOMAI_BASE_DIR}/include
        ${XENOMAI_BASE_DIR}/include/cobalt
    )

    find_library(COBALT_LIB cobalt HINTS ${XENOMAI_BASE_DIR}/lib)
    target_compile_options(${target} PRIVATE ${XENOMAI_C_FLAGS})
    target_include_directories(${target} PRIVATE ${XENOMAI_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${COBALT_LIB} rt m)
endfunction()

####################
#  Library target  #
####################

set(COMPILATION_UNITS src/twine.cpp)

# Enumerate all the headers separately so that CLion can index them
set(EXTRA_CLION_SOURCES  include/twine/twine.h
                         src/twine_internal.h
                         src/worker_pool_implementation.h
                         src/thread_helpers.h)

set(SOURCE_FILES "${COMPILATION_UNITS}" "${EXTRA_CLION_SOURCES}")

add_library(twine SHARED ${SOURCE_FILES})

target_include_directories(twine PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(twine PRIVATE pthread)

#Require users to have c++17 support
target_compile_features(twine PUBLIC cxx_std_17)
target_compile_options(twine PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(twine)
endif()

#######################
#  Unit test targets  #
#######################

if (${TWINE_WITH_TESTS})
    add_subdirectory(test)
endif()

#############
#  Install  #
#############

SET_TARGET_PROPERTIES(twine PROPERTIES VERSION 0.1)

install(TARGETS twine DESTINATION lib)
