include_guard(GLOBAL)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(
    FATAL_ERROR
      "CMake generation for '${PROJECT_NAME}' is not allowed within the source directory!\n"
      "Please create a separate directory (e.g., 'build') and run CMake from there to keep your source directory clean and organized."
  )
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")


# -----------------------------------------------------------------------------
# Set policy
cmake_policy(SET CMP0076 NEW)

# normalize paths before matching runtime dependencies
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# Use folders for source file organization with IDE generators (Visual
# Studio/Xcode)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------------------------------------
# Set compiler options

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)
if(BUILD_TYPE_LOWER STREQUAL "debug")
    set(IS_DEBUG_BUILD TRUE)
else()
    set(IS_DEBUG_BUILD FALSE)
endif()
