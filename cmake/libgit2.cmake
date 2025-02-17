include_guard(GLOBAL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libgit2 as shared library")
set(BUILD_TESTS OFF CACHE BOOL "")
set(BUILD_CLI OFF CACHE BOOL "")

add_subdirectory("${PROJECT_SOURCE_DIR}/vendor/libgit2" "${CMAKE_CURRENT_BINARY_DIR}/libgit2")

add_library(git2 INTERFACE)

target_link_libraries(git2 INTERFACE libgit2package)
target_include_directories(git2 SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/vendor/libgit2/include")
