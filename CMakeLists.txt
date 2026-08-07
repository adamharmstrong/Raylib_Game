cmake_minimum_required(VERSION 3.24...3.30)
project(RaylibGame LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
set(RAYLIB_VERSION 6.0)

FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(raylib)

# Level data is loaded at runtime, but changing it must still refresh the copy
# beside each executable.
file(GLOB_RECURSE GAME_DATA_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/game_data/*")

# Project targets
#
# Keep the reusable game code in one library so tools such as the level editor
# use exactly the same level model and parser as the game executable.
add_library(RaylibGameCore STATIC)
add_executable(${PROJECT_NAME})
add_subdirectory(src)
target_link_libraries(RaylibGameCore PUBLIC raylib)
target_link_libraries(${PROJECT_NAME} PRIVATE RaylibGameCore)

if (NOT "${PLATFORM}" STREQUAL "Web")
    add_subdirectory(level_editor)
endif()

if (BUILD_TESTING AND NOT "${PLATFORM}" STREQUAL "Web")
    add_executable(GearPhysicsTests tests/GearPhysicsTests.cpp)
    target_link_libraries(GearPhysicsTests PRIVATE RaylibGameCore)
    target_compile_definitions(GearPhysicsTests PRIVATE
        CLOCKTOWER_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/clocktower_core.level"
        GEAR_RENDER_GALLERY_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/gear_render_gallery.level")
    add_test(NAME gear_physics COMMAND GearPhysicsTests)

    add_executable(SpringPhysicsTests tests/SpringPhysicsTests.cpp)
    target_link_libraries(SpringPhysicsTests PRIVATE RaylibGameCore)
    target_compile_definitions(SpringPhysicsTests PRIVATE
        SPRING_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/spring_test_level.level"
        GEAR_GALLERY_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/gear_render_gallery.level")
    add_test(NAME spring_physics COMMAND SpringPhysicsTests)

    add_executable(GeneratorPhysicsTests tests/GeneratorPhysicsTests.cpp)
    target_link_libraries(GeneratorPhysicsTests PRIVATE RaylibGameCore)
    target_compile_definitions(GeneratorPhysicsTests PRIVATE
        COMPLETED_GATEHOUSE_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/gatehouse.level"
        GENERATOR_TEST_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/gatehouse_generator_test.level")
    add_test(NAME generator_physics COMMAND GeneratorPhysicsTests)

    add_executable(FluidPhysicsTests tests/FluidPhysicsTests.cpp)
    target_link_libraries(FluidPhysicsTests PRIVATE RaylibGameCore)
    target_compile_definitions(FluidPhysicsTests PRIVATE
        NEUROTOXIN_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/neurotoxin_maze.level")
    add_test(NAME fluid_physics COMMAND FluidPhysicsTests)

    add_executable(ButtonSequenceLevelTests tests/ButtonSequenceLevelTests.cpp)
    target_link_libraries(ButtonSequenceLevelTests PRIVATE RaylibGameCore)
    target_compile_definitions(ButtonSequenceLevelTests PRIVATE
        BUTTON_SEQUENCE_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/wendis_level_1.level")
    add_test(NAME button_sequence_level COMMAND ButtonSequenceLevelTests)

    add_executable(PortalLiftLevelTests tests/PortalLiftLevelTests.cpp)
    target_link_libraries(PortalLiftLevelTests PRIVATE RaylibGameCore)
    target_compile_definitions(PortalLiftLevelTests PRIVATE
        PORTAL_LIFT_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/wendis_level_2.level")
    add_test(NAME portal_lift_level COMMAND PortalLiftLevelTests)
endif()

# Marking these files as link dependencies makes the existing post-build copy
# run again even when no C++ source changed.
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS "${GAME_DATA_FILES}")

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if ("${PLATFORM}" STREQUAL "Web")
    add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/../assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/game_data $<TARGET_FILE_DIR:${PROJECT_NAME}>/../game_data
    )
    #DEPENDS ${PROJECT_NAME}
else()
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/game_data $<TARGET_FILE_DIR:${PROJECT_NAME}>/game_data
    )
    #DEPENDS ${PROJECT_NAME}
endif()

#set(raylib_VERBOSE 1)
# Web Configurations
if ("${PLATFORM}" STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PUBLIC -sUSE_GLFW=3 PUBLIC --preload-file assets PUBLIC --preload-file game_data)
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
