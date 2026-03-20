file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/wrapper.sh"
    INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/wrapper.in.sh"
    USE_SOURCE_PERMISSIONS
)

configure_file(
    git_env.in.sh
    git_env.sh
    @ONLY
    USE_SOURCE_PERMISSIONS
)
