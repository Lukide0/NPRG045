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
include(cmake/qt.cmake)

set(CMAKE_AUTOMOC ON)
set(INCLUDE_PATH "${CMAKE_SOURCE_DIR}/include")

# -----------------------------------------------------------------------------
# Options
option(BUILD_EXPERIMENTS "Build the experiments" ON)

# -----------------------------------------------------------------------------

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

# target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
# target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)

add_subdirectory(src)
