function(create_executable name)
    set(options DONT_INCLUDE LTO_ENABLE)
    set(oneValueArgs)
    set(multivalueArgs SOURCES LIBS)

    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multivalueArgs}")

    add_executable("${name}" ${ARG_SOURCES})

    target_compile_features("${name}" PRIVATE cxx_std_20)

    if(NOT ARG_DONT_INCLUDE)
        target_include_directories("${name}" PRIVATE "${PROJECT_SOURCE_DIR}/include")
    endif()

    set_target_properties("${name}" PROPERTIES
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR ON
        $<$<BOOL:${ARG_LTO_ENABLE} >: INTERPROCEDURAL_OPTIMIZATION TRUE>
    )

    target_link_libraries(${name} PUBLIC ${ARG_LIBS})

endfunction()
