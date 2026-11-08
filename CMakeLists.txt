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
option(BUILD_TESTS "Enable tests" OFF)
option(ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# -----------------------------------------------------------------------------

qt_standard_project_setup(
    REQUIRES 6.8
)

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

if(ENABLE_SANITIZERS)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=undefined
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
    )
endif()

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(resources)

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
# -----------------------------------------------------------------------------
# Install

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})


