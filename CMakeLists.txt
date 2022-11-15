cmake_minimum_required(VERSION 3.8)

# Set twine library version - Don't change anywhere else. Everything is generated from this.
set(TWINE_VERSION_MAJOR 0)
set(TWINE_VERSION_MINOR 3)
set(TWINE_VERSION_REVISION 0)

project(twine VERSION ${TWINE_VERSION_MAJOR}.${TWINE_VERSION_MINOR}.${TWINE_VERSION_REVISION})

########################
#  Build Options       #
########################

# _WITH_COBALT would have been a better name here, keeping the old variable for backwards compatibility
option(TWINE_WITH_XENOMAI "Build with xenomai 3.0 Cobalt realtime thread support" OFF)
option(TWINE_WITH_EVL "Build with EVL (Xenomai 4.x) realtime task support" OFF)
option(TWINE_WITH_TESTS "Build and run unit tests" ON)

if (TWINE_WITH_XENOMAI AND TWINE_WITH_EVL)
    message(FATAL_ERROR "Both Xenomai and EVL options set, choose only one of them.")
endif()

SET(TWINE_MAX_RT_CONDITION_VARS 32 CACHE STRING "The maximum number of simultaneous RtConditionVariables")

#######################
#  Cross compilation  #
#######################

if(TWINE_WITH_XENOMAI)

    set(XENOMAI_BASE_DIR "/usr/xenomai" CACHE STRING "xenomai base dir path")

    if(NOT "$ENV{CMAKE_SYSROOT}" STREQUAL "")
        set(CMAKE_SYSROOT "$ENV{CMAKE_SYSROOT}")
    endif()

    if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
        set(XENOMAI_BASE_DIR "${CMAKE_SYSROOT}/usr/xenomai")
        message("XENOMAI_BASE_DIR is " ${XENOMAI_BASE_DIR})
    endif()

endif()

##################################
#  Generate build information    #
##################################

# Get the latest commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%H
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M")

configure_file(
        ${PROJECT_SOURCE_DIR}/src/twine_version.h.in
        ${PROJECT_BINARY_DIR}/generated/twine_version.h
)

#######################
#  Utility functions  #
#######################

# Set Xenomai build options at local scope
# This intentionally does not include all the super-auto-magic linker wrappers,
# who seems to play very badly with other libraries at static initialization.
function(add_xenomai_to_target target)
    target_compile_definitions(${target} PRIVATE TWINE_BUILD_WITH_XENOMAI)

    # from `xeno-config --skin=posix --cflags`
    set(XENOMAI_C_FLAGS "-D_GNU_SOURCE -D_REENTRANT -D__COBALT__ -D__COBALT_WRAP__")

    set(XENOMAI_INCLUDE_DIRS
        ${XENOMAI_BASE_DIR}/include
        ${XENOMAI_BASE_DIR}/include/cobalt
    )

    find_library(COBALT_LIB cobalt HINTS ${XENOMAI_BASE_DIR}/lib)
    target_compile_options(${target} PRIVATE ${XENOMAI_C_FLAGS})
    target_include_directories(${target} PRIVATE ${XENOMAI_INCLUDE_DIRS})
    set(TWINE_LINK_LIBRARIES ${TWINE_LINK_LIBRARIES} ${COBALT_LIB} rt m)
endfunction()

####################
#  Library target  #
####################

set(SOURCE_FILES src/twine.cpp
                 src/thread_helpers_posix.cpp)

if (${TWINE_WITH_XENOMAI})
    set(SOURCE_FILES "${SOURCE_FILES}" src/thread_helpers_cobalt.cpp)
endif()

if (${TWINE_WITH_EVL})
    set(SOURCE_FILES "${SOURCE_FILES}" src/thread_helpers_evl.cpp)
endif()

set(TWINE_LINK_LIBRARIES pthread)

add_library(twine SHARED ${SOURCE_FILES})

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(twine)
endif()
if (${TWINE_WITH_EVL})
    message("Building Twine with EVL")
    target_compile_definitions(twine PUBLIC TWINE_BUILD_WITH_EVL)
    set(TWINE_LINK_LIBRARIES ${TWINE_LINK_LIBRARIES} evl)
endif()

target_include_directories(twine PRIVATE ${PROJECT_BINARY_DIR}/generated)
target_include_directories(twine PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(twine PUBLIC ${TWINE_LINK_LIBRARIES})

#Require users to have c++17 support
target_compile_features(twine PUBLIC cxx_std_17)
target_compile_options(twine PRIVATE -Wall -Wextra)
target_compile_definitions(twine PRIVATE TWINE_MAX_XENOMAI_RTP_DEVICES=${TWINE_MAX_RT_CONDITION_VARS})


#######################
#  Unit test targets  #
#######################

if (${TWINE_WITH_TESTS})
    add_subdirectory(test)
endif()

#############
#  Install  #
#############

set_target_properties(twine PROPERTIES VERSION "${TWINE_VERSION_MAJOR}.${TWINE_VERSION_MINOR}")
set_target_properties(twine PROPERTIES PUBLIC_HEADER include/twine/twine.h)

install(TARGETS twine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/twine
)

