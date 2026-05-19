file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/wrapper.sh"
    INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/wrapper.in.sh"
    USE_SOURCE_PERMISSIONS
)

function(generate_env OUTPUT_FILE APP_PATH)
    # temp file
    set(_tmp_env "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILE}.tmp")

    # replace @APP_PATH@
    file(CONFIGURE
        OUTPUT "${_tmp_env}"
        CONTENT [=[
#!/bin/sh

export GIT_SEQUENCE_EDITOR="@APP_PATH@"
        ]=]
        @ONLY
    )

    # evaluate generator expression
    file(GENERATE
        OUTPUT "${OUTPUT_FILE}"
        INPUT "${_tmp_env}"
    )
endfunction()


# Build env
generate_env("git_env.build.sh" "$<TARGET_FILE:${APP_NAME}>")

configure_file(
    rebase-run.in.sh
    rebase-run.sh
    @ONLY
    USE_SOURCE_PERMISSIONS
)
