target_sources(${PROJECT_NAME} PRIVATE
        RebaseViewWidget.cpp

        CommitViewWidget.cpp
        CommitMessageWidget.cpp

        ConflictWidget.cpp

        DiffWidget.cpp
        DiffEditor.cpp

        ListItem.cpp

        graph/Graph.cpp
        graph/Node.cpp
        ${INCLUDE_PATH}/gui/widget/graph/Node.h

        WelcomeWidget.cpp
        ${INCLUDE_PATH}/gui/widget/WelcomeWidget.h

        ScrollListWidget.cpp
        ${INCLUDE_PATH}/gui/widget/ScrollListWidget.h

        SettingsDialog.cpp
        ${INCLUDE_PATH}/gui/widget/SettingsDialog.h

        ${INCLUDE_PATH}/gui/widget/DiffEditor.h
        ${INCLUDE_PATH}/gui/widget/DiffFile.h

        ConflictDialog.cpp
        ${INCLUDE_PATH}/gui/widget/ConflictDialog.h

)
