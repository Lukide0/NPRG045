target_sources(${PROJECT_NAME} PRIVATE
    StyleManager.cpp

    DiffStyle.cpp
    ${INCLUDE_PATH}/gui/style/DiffStyle.h

    ConflictStyle.cpp
    ${INCLUDE_PATH}/gui/style/ConflictStyle.h
)
