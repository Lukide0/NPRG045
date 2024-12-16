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

# Use folders for source file organization with IDE generators (Visual
# Studio/Xcode)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------------------------------------
# Set compiler options

if(DEFINED CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE EQUAL "DEBUG")
    if(NOT DEFINED CMAKE_COMPILE_WARNING_AS_ERROR)
        set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
    endif()
endif()
