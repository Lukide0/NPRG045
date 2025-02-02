create_executable("rebase" SOURCES rebase/main.cpp
    DONT_INCLUDE
    LIBS git2
)

target_include_directories("rebase" PRIVATE common)
