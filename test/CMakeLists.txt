# build gtest framework
add_subdirectory(gtest)
enable_testing()

#####################################
#  Build Flags                      #
#####################################

# Overwrite parent definition to avoid issues with Xenomai wrappers
set(CMAKE_EXE_LINKER_FLAGS "")

#####################
#  Unit Test Files  #
#####################

remove_definitions(-DTWINE_BUILD_WITH_XENOMAI)
SET(TEST_FILES unittests/twine_tests.cpp
               unittests/std_worker_pool_tests.cpp
               ../src/xenomai_worker_pool.cpp)

set(INCLUDE_DIRS "${INCLUDE_DIRS}"
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test/unittests
        ${PROJECT_SOURCE_DIR}/test/gtest/include)

include_directories(${INCLUDE_DIRS})

#################################
#  Statically linked libraries  #
#################################

set(TEST_LINK_LIBRARIES
        gtest
        gtest_main        )

add_executable(unit_tests ${TEST_FILES})

target_link_libraries(unit_tests "${TEST_LINK_LIBRARIES}")
add_test(unit_tests unit_tests)

### Custom target for running the tests
# Environment variable pointing to test/data/ is set so that
# tests can read it to access data files maintaining an independent out-of-source build

add_custom_target(run_tests ALL
        ${CMAKE_COMMAND}
        -E env "SUSHI_TEST_DATA_DIR=${PROJECT_SOURCE_DIR}/test/data"
        "./unit_tests")
add_dependencies(run_tests unit_tests)

