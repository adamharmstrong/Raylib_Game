add_executable(LevelEditor
    main.cpp
    LevelEditor.cpp
    LevelEditor.h
)

target_link_libraries(LevelEditor PRIVATE RaylibGameCore)

set_target_properties(LevelEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/LevelEditor"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:LevelEditor>"
)

set_property(TARGET LevelEditor APPEND PROPERTY LINK_DEPENDS "${GAME_DATA_FILES}")

add_custom_command(
    TARGET LevelEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LevelEditor>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/game_data"
        "$<TARGET_FILE_DIR:LevelEditor>/game_data"
)
