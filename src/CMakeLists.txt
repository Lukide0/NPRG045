target_sources(${PROJECT_NAME} PRIVATE
        main.cpp
        App.cpp
)

add_subdirectory(gui)
add_subdirectory(action)
add_subdirectory(logging)
add_subdirectory(git)
add_subdirectory(state)
add_subdirectory(patch)
add_subdirectory(conflict)
