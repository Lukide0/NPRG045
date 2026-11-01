target_sources(${PROJECT_NAME} PRIVATE
        ${INCLUDE_PATH}/gui/widget/RebaseViewWidget.h
        RebaseViewWidget.cpp

        ${INCLUDE_PATH}/gui/widget/CommitViewWidget.h
        CommitViewWidget.cpp

        ${INCLUDE_PATH}/gui/widget/CommitMessageWidget.h
        CommitMessageWidget.cpp

        ${INCLUDE_PATH}/gui/widget/RebaseSelectionWidget.h
        RebaseSelectionWidget.cpp

        ${INCLUDE_PATH}/gui/widget/ConflictWidget.h
        ConflictWidget.cpp

        ${INCLUDE_PATH}/gui/widget/DiffWidget.h
        DiffWidget.cpp

        DiffEditor.cpp

        WelcomeWidget.cpp
        ${INCLUDE_PATH}/gui/widget/WelcomeWidget.h

        ScrollListWidget.cpp
        ${INCLUDE_PATH}/gui/widget/ScrollListWidget.h

        SettingsDialog.cpp
        ${INCLUDE_PATH}/gui/widget/SettingsDialog.h

        ShortcutEditor.cpp
        ${INCLUDE_PATH}/gui/widget/ShortcutEditor.h

        ${INCLUDE_PATH}/gui/widget/DiffEditor.h
        ${INCLUDE_PATH}/gui/widget/DiffFile.h

        ConflictDialog.cpp
        ${INCLUDE_PATH}/gui/widget/ConflictDialog.h

)
