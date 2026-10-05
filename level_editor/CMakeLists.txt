add_executable(LevelEditorViewport
    main.cpp
    LevelEditor.cpp
    LevelEditor.h
    EditorConnections.cpp
    EditorConnections.h
)

target_link_libraries(LevelEditorViewport PRIVATE RaylibGameCore)

if (WIN32)
    target_link_libraries(LevelEditorViewport PRIVATE comdlg32)
endif()

set_target_properties(LevelEditorViewport PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/LevelEditor"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/LevelEditor"
)

set_property(TARGET LevelEditorViewport APPEND PROPERTY LINK_DEPENDS "${GAME_DATA_FILES}")

add_custom_command(
    TARGET LevelEditorViewport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${CMAKE_BINARY_DIR}/LevelEditor/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/game_data"
        "${CMAKE_BINARY_DIR}/LevelEditor/game_data"
)

if (WIN32)
    add_custom_target(LevelEditor ALL
        COMMAND dotnet build "${CMAKE_SOURCE_DIR}/level_editor_winforms/LevelEditor.WinForms.csproj"
            --configuration Debug
            --output "${CMAKE_BINARY_DIR}/LevelEditor"
            --nologo
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:RaylibGame> "${CMAKE_BINARY_DIR}/LevelEditor/RaylibGame.exe"
        DEPENDS LevelEditorViewport RaylibGame
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building the WinForms level editor shell"
        VERBATIM
    )
endif()
