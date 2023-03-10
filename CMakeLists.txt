cmake_minimum_required(VERSION 3.8)

# Set twine library version - Don't change anywhere else. Everything is generated from this.
set(TWINE_VERSION_MAJOR 0)
set(TWINE_VERSION_MINOR 3)
set(TWINE_VERSION_REVISION 3)

project(twine
    DESCRIPTION "Thread and Worker Interface for Elk Audio OS"
    HOMEPAGE_URL "https://github.com/elk-audio/twine"
    LANGUAGES CXX
    VERSION ${TWINE_VERSION_MAJOR}.${TWINE_VERSION_MINOR}.${TWINE_VERSION_REVISION}
)

###########################
#  Build and link options #
###########################

# The defaults enable all options and select APIs available for either Xenomai or macOS
set(TWINE_WITH_XENOMAI_DEFAULT OFF)
set(TWINE_WITH_TESTS_DEFAULT ON)

if (APPLE)
    set(EXTRA_COMPILE_DEFINITIONS ${EXTRA_COMPILE_DEFINITIONS} -DTWINE_APPLE_THREADING)
    set(TWINE_WITH_APPLE_COREAUDIO_DEFAULT ON)
else()
    set(TWINE_WITH_APPLE_COREAUDIO_DEFAULT OFF)
endif()

########################
#  Build Options       #
########################

option(TWINE_WITH_XENOMAI "Build with xenomai realtime thread support" ${TWINE_WITH_XENOMAI_DEFAULT})
option(TWINE_WITH_TESTS "Build and run unit tests" ${TWINE_WITH_TESTS_DEFAULT})
option(TWINE_WITH_APPLE_COREAUDIO "Enable Jack support" ${TWINE_WITH_APPLE_COREAUDIO_DEFAULT})

set(TWINE_MAX_RT_CONDITION_VARS 32 CACHE STRING "The maximum number of simultaneous RtConditionVariables")

###################
# CoreAudio setup #
###################
if (${TWINE_WITH_APPLE_COREAUDIO})
    message("Building with Apple CoreAudio support.")
    # Compile definitions
    set(EXTRA_COMPILE_DEFINITIONS ${EXTRA_COMPILE_DEFINITIONS} -DTWINE_BUILD_WITH_APPLE_COREAUDIO)
    set(EXTRA_BUILD_LIBRARIES ${EXTRA_BUILD_LIBRARIES} "-framework CoreAudio -framework Foundation")
endif()

#######################
#  Cross compilation  #
#######################

set(XENOMAI_BASE_DIR "/usr/xenomai" CACHE STRING "xenomai base dir path")

if(NOT "$ENV{CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "$ENV{CMAKE_SYSROOT}")
endif()

if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(XENOMAI_BASE_DIR "${CMAKE_SYSROOT}/usr/xenomai")
    message("XENOMAI_BASE_DIR is " ${XENOMAI_BASE_DIR})
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
    target_compile_definitions(${target} PRIVATE -DTWINE_BUILD_WITH_XENOMAI)

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

function(set_twine_target_properties target)
    target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR}/generated)
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(${target} PRIVATE pthread ${EXTRA_BUILD_LIBRARIES})
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE 1)
    target_compile_definitions(${target} PUBLIC ${EXTRA_COMPILE_DEFINITIONS} PRIVATE TWINE_MAX_XENOMAI_RTP_DEVICES=${TWINE_MAX_RT_CONDITION_VARS})
    if (${TWINE_WITH_XENOMAI})
        add_xenomai_to_target(${target})
    endif()
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    set_target_properties(${target} PROPERTIES VERSION "${TWINE_VERSION_MAJOR}.${TWINE_VERSION_MINOR}")
    set_target_properties(${target} PROPERTIES PUBLIC_HEADER include/twine/twine.h)
endfunction()

####################
#  Library target  #
####################

set(SOURCE_FILES src/twine.cpp
                 src/apple_threading.cpp)

# The best way to build both static & dynamic targets
# would have been to reuse the existing objects as in:
#   add_library(twine SHARED $<TARGET_OBJECTS:twine_objlib>)
# but it doesn't play nicely with XCode builds, so we just define both

add_library(twine_static STATIC ${SOURCE_FILES})
set_twine_target_properties(twine_static)
add_library(twine SHARED ${SOURCE_FILES})
set_twine_target_properties(twine)

#######################
#  Unit test targets  #
#######################

if (${TWINE_WITH_TESTS})
    add_subdirectory(test)
endif()

#############
#  Install  #
#############

install(TARGETS twine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/twine
)

