# build gtest framework
if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(gtest EXCLUDE_FROM_ALL)
enable_testing()

# Overwrite parent definition to avoid issues with Xenomai wrappers
set(CMAKE_EXE_LINKER_FLAGS "")

#####################
#  Unit Test Files  #
#####################

set(TEST_COMPILE_DEFINITIONS)

SET(TEST_FILES unittests/twine_tests.cpp
               unittests/condition_variable_test.cpp)

if(NOT MSVC)
    set(TEST_FILES ${TEST_FILES} unittests/worker_pool_tests.cpp)
endif()

set(INCLUDE_DIRS ${INCLUDE_DIRS}
                 ${PROJECT_SOURCE_DIR}/include
                 ${PROJECT_SOURCE_DIR}/src
                 ${PROJECT_SOURCE_DIR}/test/gtest/include
                 ${CMAKE_BINARY_DIR}/generated)

set(TEST_LINK_LIBRARIES
        gtest
        gtest_main
        gmock
        gmock_main
        twine)
## TODO - test should not link with twine (this pulls in evl/xenomai)

# Unit tests are build without xenomai/evl regardless
remove_definitions(-DTWINE_BUILD_WITH_XENOMAI)
remove_definitions(-DTWINE_BUILD_WITH_EVL)

if (${TWINE_WITH_APPLE_COREAUDIO})
    set(TEST_FILES ${TEST_FILES}
            test_utils/apple_coreaudio_mockup.cpp)

    set(TEST_COMPILE_DEFINITIONS ${TEST_COMPILE_DEFINITIONS} -DTWINE_BUILD_WITH_APPLE_COREAUDIO)
    set(TEST_LINK_LIBRARIES ${TEST_LINK_LIBRARIES} "-framework CoreAudio  -framework Foundation")
endif()

add_executable(unit_tests ${TEST_FILES})

target_compile_definitions(unit_tests PRIVATE ${TEST_COMPILE_DEFINITIONS} TWINE_MAX_RT_CONDITION_VARS=${TWINE_MAX_RT_CONDITION_VARS} TWINE_EXPOSE_INTERNALS)

if(NOT MSVC)
    target_compile_options(unit_tests PRIVATE -Wall -Wextra)
endif()

target_link_libraries(unit_tests PRIVATE "${TEST_LINK_LIBRARIES}")
target_link_libraries(unit_tests INTERFACE $<BUILD_INTERFACE:elk::warningSuppressor>)
target_include_directories(unit_tests PRIVATE ${INCLUDE_DIRS})

target_compile_features(unit_tests PRIVATE cxx_std_20)

if (${TWINE_WITH_XENOMAI})
    target_compile_definitions(unit_tests PRIVATE TWINE_BUILD_XENOMAI_TESTS)
    target_link_libraries(unit_tests PRIVATE ${COBALT_LIB})
endif()
if (${TWINE_WITH_EVL})
    target_link_libraries(unit_tests PRIVATE evl)
endif()

add_test(unit_tests unit_tests)

### Custom target for running the tests

add_custom_target(run_tests ALL COMMAND "$<TARGET_FILE:unit_tests>")
add_dependencies(run_tests unit_tests)

# Stress test is not built per default
add_subdirectory(stresstest EXCLUDE_FROM_ALL)
