set(CMAKE_EXE_LINKER_FLAGS "")

add_executable(pool_stress_test pool_stresstest.cpp)
target_link_libraries(pool_stress_test PRIVATE twine pthread)
target_include_directories(pool_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(pool_stress_test PRIVATE cxx_std_17)
target_compile_options(pool_stress_test PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(pool_stress_test)
endif()


add_executable(condition_variable_stress_test cond_var_stresstest.cpp)
target_link_libraries(condition_variable_stress_test PRIVATE twine pthread)
target_include_directories(condition_variable_stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(condition_variable_stress_test PRIVATE cxx_std_17)
target_compile_options(condition_variable_stress_test PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(condition_variable_stress_test)
endif()
