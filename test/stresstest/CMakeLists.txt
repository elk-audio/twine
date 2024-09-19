set(CMAKE_EXE_LINKER_FLAGS "")

set(STRESS_TEST_LINK_LIBRARIES twine)

if (${TWINE_WITH_EVL})
    set(STRESS_TEST_LINK_LIBRARIES ${STRESS_TEST_LINK_LIBRARIES} evl)
endif()

if (MSVC)
    set(STRESS_TEST_COMPILE_OPTIONS -Wall)
else()
    set(STRESS_TEST_COMPILE_OPTIONS -Wall -Wextra)
endif()

add_executable(pool_stress_test pool_stresstest.cpp)
target_link_libraries(pool_stress_test PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
target_include_directories(pool_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(pool_stress_test PRIVATE cxx_std_20)
target_compile_options(pool_stress_test PRIVATE ${STRESS_TEST_COMPILE_OPTIONS})

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(pool_stress_test)
endif()

add_executable(condition_variable_stress_test cond_var_stresstest.cpp)
target_link_libraries(condition_variable_stress_test PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
target_include_directories(condition_variable_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test/test_utils)
target_compile_features(condition_variable_stress_test PRIVATE cxx_std_20)
target_compile_options(condition_variable_stress_test PRIVATE ${STRESS_TEST_COMPILE_OPTIONS})

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(condition_variable_stress_test)
endif()

### Custom command to copy twine.dll to the binary folder after build
if (MSVC)
    add_custom_command(TARGET condition_variable_stress_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:condition_variable_stress_test> $<TARGET_FILE_DIR:condition_variable_stress_test>
            COMMAND_EXPAND_LISTS)
endif()

if (${TWINE_WITH_EVL})
    add_executable(cond_var_full_duplex_test cond_var_full_duplex_test.cpp)
    target_link_libraries(cond_var_full_duplex_test PRIVATE PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
    target_include_directories(cond_var_full_duplex_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_features(cond_var_full_duplex_test PRIVATE cxx_std_20)
    target_compile_options(cond_var_full_duplex_test PRIVATE ${STRESS_TEST_COMPILE_OPTIONS})
endif()
