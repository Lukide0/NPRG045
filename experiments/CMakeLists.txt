create_executable("rebase" SOURCES rebase/main.cpp
    DONT_INCLUDE
    LIBS git2
)

target_include_directories("rebase" PRIVATE common)


create_executable("sequencer"
    SOURCES
        sequencer/main.cpp
        sequencer/App.cpp
        sequencer/RebaseViewWidget.cpp

    DONT_INCLUDE
    LIBS git2 ${QT_WIDGETS})

target_include_directories("sequencer" PRIVATE common)

