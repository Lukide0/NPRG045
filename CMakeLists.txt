cmake_minimum_required(VERSION 3.25)
project(
    git_shuffle
    LANGUAGES CXX
)

include(cmake/bootstrap.cmake)
include(cmake/create_executable.cmake)

# -----------------------------------------------------------------------------
# Libs

include(cmake/libgit2.cmake)

# -----------------------------------------------------------------------------
# Options
option(BUILD_EXPERIMENTS "Build the experiments" ON)

# -----------------------------------------------------------------------------

if(BUILD_EXPERIMENTS)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/experiments")
endif()
