set(CMAKE_EXE_LINKER_FLAGS "")

add_executable(stress_test stresstest.cpp)
target_link_libraries(stress_test PRIVATE twine pthread)
target_include_directories(stress_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(stress_test PRIVATE cxx_std_17)
target_compile_options(stress_test PRIVATE -Wall -Wextra)

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(stress_test)
endif()
