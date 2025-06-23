target_sources(${PROJECT_NAME} PRIVATE
        RebaseViewWidget.cpp

        CommitViewWidget.cpp
        CommitMessageWidget.cpp

        ConflictWidget.cpp
        ConflictEditor.cpp
        ${INCLUDE_PATH}/gui/widget/ConflictFile.h

        DiffWidget.cpp
        DiffEditor.cpp

        ListItem.cpp

        graph/Graph.cpp
        graph/Node.cpp

        ${INCLUDE_PATH}/gui/widget/DiffEditor.h
        ${INCLUDE_PATH}/gui/widget/DiffFile.h
)
