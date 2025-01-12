create_executable("rebase" SOURCES rebase/main.cpp
    DONT_INCLUDE
    LIBS git2
)

create_executable("sequencer" SOURCES sequencer/main.cpp
    DONT_INCLUDE
    LIBS git2 ${QT_WIDGETS})


