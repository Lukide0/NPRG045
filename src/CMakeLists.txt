target_sources(${PROJECT_NAME} PRIVATE
        main.cpp
        App.cpp
)

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(action)
