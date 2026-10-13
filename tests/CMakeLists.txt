include(CTest)

enable_testing()

# Parser test
add_executable(rebase_parser_tests
    parser.test.cpp
    ${PROJECT_SOURCE_DIR}/src/git/parser.cpp
)
target_include_directories(rebase_parser_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/vendor/doctest
)
target_compile_features(rebase_parser_tests PRIVATE cxx_std_20)

add_test(
    NAME rebase_parser_tests
    COMMAND rebase_parser_tests
)
