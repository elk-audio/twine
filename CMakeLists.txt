cmake_minimum_required(VERSION 3.8)
project(twine)

########################
#  Build configuration #
########################

#set(WITH_APPS true CACHE BOOL "Build included applications")
set(WITH_TESTS true CACHE BOOL "Build and run unittests")
set(WITH_XENOMAI true CACHE BOOL "Build with xenomai realtime thread support")

####################
#  Compiler Flags  #
####################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global flags
set(PROJECT_C_FLAGS "-Wall -Wextra")

# Build optimization flags - these are the same for all targets
set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -O3 -ftree-vectorize -ffast-math -m64 -march=corei7 -mtune=corei7 -mfpmath=sse -msse4.2")
set(CMAKE_C_FLAGS_DEBUG   "-DDEBUG -O0 -g")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PROJECT_C_FLAGS}")

#########################
#  Include Directories  #
#########################

set(INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
)

include_directories(${INCLUDE_DIRS})

#######################
#  Utility functions  #
#######################

# Set Xenomai build options at local scope
# This intentionally does not include all the super-auto-magic linker wrappers,
# who seems to play very badly with other libraries at static initialization.
function(add_xenomai_to_target target)
    set(XENOMAI_BASE_DIR /usr/xenomai)

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
        src/worker_pool.cpp
)

if (${WITH_XENOMAI})
    set(COMPILATION_UNITS ${COMPILATION_UNITS}
            src/xenomai_worker_pool.cpp)
endif()

# Enumerate all the headers separately so that CLion can index them
set(EXTRA_CLION_SOURCES
        include/twine.h
        include/worker_pool.h
        src/std_worker_pool.h
        src/xenomai_worker_pool.h
)

set(SOURCE_FILES "${COMPILATION_UNITS}" "${EXTRA_CLION_SOURCES}")

add_library(yarn SHARED ${SOURCE_FILES})
if (${WITH_XENOMAI})
    add_xenomai_to_target(twine)
endif()


#############################################
#  Subdirectory projects                    #
#############################################

#if (${WITH_APPS})
#    add_subdirectory(apps)
#endif()
#if (${WITH_TESTS})
#    add_subdirectory(test)
#endif()
