target_sources(${PROJECT_NAME} PRIVATE
    ${INCLUDE_PATH}/gui/git/Graph.h
    Graph.cpp

    ${INCLUDE_PATH}/gui/git/GraphPresenter.h
    GraphPresenter.cpp

    ${INCLUDE_PATH}/gui/git/CommitViewWidget.h
    CommitViewWidget.cpp

    ${INCLUDE_PATH}/gui/git/CommitMessageWidget.h
    CommitMessageWidget.cpp

    ${INCLUDE_PATH}/gui/git/ConflictWidget.h
    ConflictWidget.cpp

    ${INCLUDE_PATH}/gui/git/DiffWidget.h
    DiffWidget.cpp

    ${INCLUDE_PATH}/gui/git/DiffEditor.h
    ${INCLUDE_PATH}/gui/git/DiffFile.h

    DiffEditor.cpp


)

