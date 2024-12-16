create_executable("rebase" SOURCES rebase/main.cpp
    DONT_INCLUDE
    LIBS git2
)
