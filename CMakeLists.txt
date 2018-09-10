cmake_minimum_required(VERSION 3.8)
project(twine)

########################
#  Build configuration #
########################

set(TWINE_WITH_TESTS true CACHE BOOL "Build and run unittests")
set(TWINE_WITH_XENOMAI true CACHE BOOL "Build with xenomai realtime thread support")
set(XENOMAI_BASE_DIR "/usr/xenomai" CACHE STRING "xenomai base dir path")

####################
#  Compiler Flags  #
####################

if(NOT "$ENV{CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "$ENV{CMAKE_SYSROOT}")
endif()

if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(XENOMAI_BASE_DIR "${CMAKE_SYSROOT}/usr/xenomai")
    message("XENOMAI_BASE_DIR is " ${XENOMAI_BASE_DIR})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global flags
set(PROJECT_CXX_FLAGS "-Wall -Wextra")

# Build optimization flags - these are the same for all targets
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 ")
set(CMAKE_CXX_FLAGS_DEBUG   "-DDEBUG -O0 -g")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PROJECT_CXX_FLAGS}")

#########################
#  Include Directories  #
#########################

set(INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
)

#######################
#  Utility functions  #
#######################

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

    set(XENOMAI_LINK_LIBRARIES ${COBALT_LIB} pthread rt m)

    target_compile_options(${target} PRIVATE ${XENOMAI_C_FLAGS})
    target_include_directories(${target} PRIVATE ${XENOMAI_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${XENOMAI_LINK_LIBRARIES})
endfunction()

##################
#  Source Files  #
##################

set(COMPILATION_UNITS
        src/twine.cpp
)

# Enumerate all the headers separately so that CLion can index them
set(EXTRA_CLION_SOURCES
        include/twine.h
        src/twine_internal.h
        src/worker_pool_implementation.h
        src/thread_helpers.h
)

set(SOURCE_FILES "${COMPILATION_UNITS}" "${EXTRA_CLION_SOURCES}")

add_library(twine SHARED ${SOURCE_FILES})
target_include_directories(twine PRIVATE ${INCLUDE_DIRS})

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(twine)
endif()


#############################################
#  Subdirectory projects                    #
#############################################

if (${WITH_TESTS})
    add_subdirectory(test)
endif()

#############
#  Install  #
#############

install(TARGETS twine DESTINATION lib)

