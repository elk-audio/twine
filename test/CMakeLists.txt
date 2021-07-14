# build gtest framework
add_subdirectory(gtest EXCLUDE_FROM_ALL)
enable_testing()

# Overwrite parent definition to avoid issues with Xenomai wrappers
set(CMAKE_EXE_LINKER_FLAGS "")

#####################
#  Unit Test Files  #
#####################

# Unit tests are build without xenomai regardles
remove_definitions(-DTWINE_BUILD_WITH_XENOMAI)

add_executable(unit_tests unittests/twine_tests.cpp
                          unittests/worker_pool_tests.cpp
                          unittests/condition_variable_test.cpp)

target_include_directories(unit_tests PRIVATE  ${PROJECT_SOURCE_DIR}/include
                                               ${PROJECT_SOURCE_DIR}/src
                                               ${PROJECT_SOURCE_DIR}/test/gtest/include
                                               ${CMAKE_BINARY_DIR}/generated)


target_link_libraries(unit_tests gtest gtest_main twine)

target_compile_features(unit_tests PRIVATE cxx_std_17)
target_compile_options(unit_tests PRIVATE -Wall -Wextra)
if (${TWINE_WITH_XENOMAI})
    target_compile_definitions(unit_tests PRIVATE TWINE_BUILD_XENOMAI_TESTS)
endif()

add_test(unit_tests unit_tests)

### Custom target for running the tests

add_custom_target(run_tests ALL COMMAND "./unit_tests")
add_dependencies(run_tests unit_tests)

# Stress test is not built per default
add_subdirectory(stresstest EXCLUDE_FROM_ALL)



