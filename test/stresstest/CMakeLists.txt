
set(CMAKE_EXE_LINKER_FLAGS "")

#####################
#  Source Files  #
#####################

SET(SOURCE_FILES stresstest.cpp)

set(INCLUDE_DIRS "${INCLUDE_DIRS}"
        ${PROJECT_SOURCE_DIR}/src)

include_directories(${INCLUDE_DIRS})

set(LINK_LIBRARIES twine pthread)

add_executable(stress_test ${SOURCE_FILES})

if (${TWINE_WITH_XENOMAI})
    add_xenomai_to_target(stress_test)
endif()
target_link_libraries(stress_test PRIVATE "${LINK_LIBRARIES}")


add_dependencies(stress_test twine)