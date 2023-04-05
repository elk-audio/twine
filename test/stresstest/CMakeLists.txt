set(CMAKE_EXE_LINKER_FLAGS "")

set(STRESS_TEST_LINK_LIBRARIES pthread twine)

if (${TWINE_WITH_EVL})
    set(STRESS_TEST_LINK_LIBRARIES ${STRESS_TEST_LINK_LIBRARIES} evl)
endif()

add_executable(pool_stress_test pool_stresstest.cpp)
target_link_libraries(pool_stress_test PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
target_include_directories(pool_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(pool_stress_test PRIVATE cxx_std_17)
target_compile_options(pool_stress_test PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(pool_stress_test)
endif()


add_executable(condition_variable_stress_test cond_var_stresstest.cpp)
target_link_libraries(condition_variable_stress_test PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
target_include_directories(condition_variable_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(condition_variable_stress_test PRIVATE cxx_std_17)
target_compile_options(condition_variable_stress_test PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(condition_variable_stress_test)
endif()


add_executable(cond_var_full_duplex_test cond_var_full_duplex_test.cpp)
target_link_libraries(cond_var_full_duplex_test PRIVATE PRIVATE ${STRESS_TEST_LINK_LIBRARIES})
target_include_directories(cond_var_full_duplex_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(cond_var_full_duplex_test PRIVATE cxx_std_17)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(cond_var_full_duplex_test)
endif()
