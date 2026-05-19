cmake_minimum_required(VERSION 3.25)
project(
    git_shuffle
    VERSION 1.0.0
    LANGUAGES CXX
)

include(cmake/bootstrap.cmake)
include(cmake/create_executable.cmake)

set(APP_NAME "${PROJECT_NAME}")

# -----------------------------------------------------------------------------
# Libs

include(cmake/libgit2.cmake)
include(cmake/qt.cmake)

set(INCLUDE_PATH "${CMAKE_SOURCE_DIR}/include")

# -----------------------------------------------------------------------------
# Options
option(BUILD_EXPERIMENTS "Build the experiments" ON)

# -----------------------------------------------------------------------------

qt_standard_project_setup(
    REQUIRES 6.8
)

if(BUILD_EXPERIMENTS)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/experiments")
endif()


create_executable(${PROJECT_NAME}
    LIBS
        git2
        ${QT_WIDGETS}
        ${QT_CORE}
        ${QT_XML}
)


target_compile_definitions(${PROJECT_NAME} PRIVATE
    SOURCE_ROOT="${CMAKE_SOURCE_DIR}/"
    GIT_SHUFFLE_VERSION="$<IF:$<CONFIG:Debug>,${PROJECT_VERSION}-debug,${PROJECT_VERSION}>"
    APP_NAME="${APP_NAME}"
)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(resources)
