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

# Vendored Lua keeps native and web builds independent of a system install.
file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/third_party/lua-5.4.8/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(GameLua STATIC ${LUA_SOURCES})
target_include_directories(GameLua PUBLIC "${CMAKE_SOURCE_DIR}/third_party/lua-5.4.8/src")
if(UNIX AND NOT EMSCRIPTEN)
    target_link_libraries(GameLua PUBLIC m)
endif()

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
target_link_libraries(RaylibGameCore PUBLIC GameLua)
target_include_directories(RaylibGameCore PRIVATE "${CMAKE_SOURCE_DIR}/third_party")
target_include_directories(RaylibGameCore PRIVATE "${CMAKE_SOURCE_DIR}/script_sdk")
target_link_libraries(${PROJECT_NAME} PRIVATE RaylibGameCore)

if (NOT "${PLATFORM}" STREQUAL "Web")
    add_subdirectory(level_editor)
endif()

if (BUILD_TESTING AND NOT "${PLATFORM}" STREQUAL "Web")
    add_executable(CustomLevelTests tests/CustomLevelTests.cpp)
    target_link_libraries(CustomLevelTests PRIVATE RaylibGameCore)
    add_test(NAME custom_levels COMMAND CustomLevelTests)
    add_executable(EditorRotationTests tests/EditorRotationTests.cpp level_editor/LevelEditor.cpp level_editor/EditorConnections.cpp)
    target_include_directories(EditorRotationTests PRIVATE "${CMAKE_SOURCE_DIR}/level_editor")
    target_link_libraries(EditorRotationTests PRIVATE RaylibGameCore)
    if (WIN32)
        target_link_libraries(EditorRotationTests PRIVATE comdlg32)
    endif()
    add_test(NAME editor_rotation COMMAND EditorRotationTests)
    if(WIN32)
        add_library(CppScriptFixture SHARED tests/fixtures/script_behavior.cpp)
        target_include_directories(CppScriptFixture PRIVATE script_sdk)
        set_target_properties(CppScriptFixture PROPERTIES PREFIX "" OUTPUT_NAME "script-fixture.cpp" SUFFIX ".dll")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(CppScriptFixture PRIVATE -static-libgcc -static-libstdc++)
        endif()
        add_executable(CppScriptTests tests/CppScriptTests.cpp)
        target_link_libraries(CppScriptTests PRIVATE RaylibGameCore)
        target_compile_definitions(CppScriptTests PRIVATE CPP_FIXTURE_PATH="$<TARGET_FILE:CppScriptFixture>")
        add_dependencies(CppScriptTests CppScriptFixture)
        add_test(NAME cpp_scripts COMMAND CppScriptTests)
    endif()
    add_executable(LuaScriptTests tests/LuaScriptTests.cpp)
    target_link_libraries(LuaScriptTests PRIVATE RaylibGameCore)
    add_test(NAME lua_scripts COMMAND LuaScriptTests)
    add_executable(WedgeTests tests/WedgeTests.cpp)
    target_link_libraries(WedgeTests PRIVATE RaylibGameCore)
    target_compile_definitions(WedgeTests PRIVATE WEDGE_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures/wedges.level")
    add_test(NAME wedges COMMAND WedgeTests)
    add_executable(LadderCollisionTests tests/LadderCollisionTests.cpp)
    target_link_libraries(LadderCollisionTests PRIVATE RaylibGameCore)
    add_test(NAME ladder_collision COMMAND LadderCollisionTests)
    add_executable(LoopEditorTests tests/LoopEditorTests.cpp)
    target_include_directories(LoopEditorTests PRIVATE "${CMAKE_SOURCE_DIR}/level_editor")
    add_executable(SpriteSheetTests tests/SpriteSheetTests.cpp)
    target_link_libraries(SpriteSheetTests PRIVATE RaylibGameCore)
    add_test(NAME sprite_sheets COMMAND SpriteSheetTests)
    add_executable(TileSelectionTransformTests tests/TileSelectionTransformTests.cpp)
    target_include_directories(TileSelectionTransformTests PRIVATE "${CMAKE_SOURCE_DIR}/level_editor")
    target_link_libraries(TileSelectionTransformTests PRIVATE RaylibGameCore)
    add_test(NAME tile_selection_transforms COMMAND TileSelectionTransformTests)
    target_link_libraries(LoopEditorTests PRIVATE RaylibGameCore)
    target_compile_definitions(LoopEditorTests PRIVATE
        LOOP_EXPLICIT_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures/loop_editor_explicit.level"
        LOOP_LEGACY_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures/loop_editor_legacy.level")
    add_test(NAME loop_editor COMMAND LoopEditorTests)
    add_executable(EditorPlacementTests tests/EditorPlacementTests.cpp)
    target_include_directories(EditorPlacementTests PRIVATE "${CMAKE_SOURCE_DIR}/level_editor")
    target_link_libraries(EditorPlacementTests PRIVATE RaylibGameCore)
    add_test(NAME editor_placement COMMAND EditorPlacementTests)

    add_executable(EditorConnectionTests tests/EditorConnectionTests.cpp level_editor/EditorConnections.cpp)
    target_include_directories(EditorConnectionTests PRIVATE "${CMAKE_SOURCE_DIR}/level_editor")
    target_link_libraries(EditorConnectionTests PRIVATE RaylibGameCore)
    add_test(NAME editor_connections COMMAND EditorConnectionTests)

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

    add_executable(AntiGravityPadTests tests/AntiGravityPadTests.cpp)
    target_link_libraries(AntiGravityPadTests PRIVATE RaylibGameCore)
    target_compile_definitions(AntiGravityPadTests PRIVATE
        TEST_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/test_level.level")
    add_test(NAME anti_gravity_pad COMMAND AntiGravityPadTests)

    add_executable(TeleforceTests tests/TeleforceTests.cpp)
    target_link_libraries(TeleforceTests PRIVATE RaylibGameCore)
    add_test(NAME teleforce COMMAND TeleforceTests)

    add_executable(TeslaCoilTests tests/TeslaCoilTests.cpp)
    target_link_libraries(TeslaCoilTests PRIVATE RaylibGameCore)
    add_test(NAME tesla_coil COMMAND TeslaCoilTests)

    add_executable(ThornyVinesTests tests/ThornyVinesTests.cpp)
    target_link_libraries(ThornyVinesTests PRIVATE RaylibGameCore)
    target_compile_definitions(ThornyVinesTests PRIVATE
        WATER_ESCAPE_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/kirils_level_3.level")
    add_test(NAME thorny_vines COMMAND ThornyVinesTests)

    add_executable(LevelAuthorTests tests/LevelAuthorTests.cpp)
    target_link_libraries(LevelAuthorTests PRIVATE RaylibGameCore)
    target_compile_definitions(LevelAuthorTests PRIVATE
        LEVEL_DIRECTORY="${CMAKE_SOURCE_DIR}/game_data/levels")
    add_test(NAME level_authors COMMAND LevelAuthorTests)

    add_executable(LeedskalninDeviceTests tests/LeedskalninDeviceTests.cpp)
    target_link_libraries(LeedskalninDeviceTests PRIVATE RaylibGameCore)
    target_compile_definitions(LeedskalninDeviceTests PRIVATE
        LEEDSKALNIN_WORKSHOP_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/leedskalnin_workshop.level")
    add_test(NAME leedskalnin_devices COMMAND LeedskalninDeviceTests)

    add_executable(ClassicContraptionTests tests/ClassicContraptionTests.cpp)
    target_link_libraries(ClassicContraptionTests PRIVATE RaylibGameCore)
    target_compile_definitions(ClassicContraptionTests PRIVATE
        CLASSIC_LAB_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/incredible_machine_lab.level")
    add_test(NAME classic_contraptions COMMAND ClassicContraptionTests)

    add_executable(ThermalObjectTests tests/ThermalObjectTests.cpp)
    target_link_libraries(ThermalObjectTests PRIVATE RaylibGameCore)
    add_test(NAME thermal_objects COMMAND ThermalObjectTests)

    add_executable(SignalDustTests tests/SignalDustTests.cpp)
    target_link_libraries(SignalDustTests PRIVATE RaylibGameCore)
    add_test(NAME signal_dust COMMAND SignalDustTests)

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

    add_executable(CounterweightRowLevelTests tests/CounterweightRowLevelTests.cpp)
    target_link_libraries(CounterweightRowLevelTests PRIVATE RaylibGameCore)
    target_compile_definitions(CounterweightRowLevelTests PRIVATE
        COUNTERWEIGHT_ROW_LEVEL_PATH="${CMAKE_SOURCE_DIR}/game_data/levels/counterweight_row.level")
    add_test(NAME counterweight_row_level COMMAND CounterweightRowLevelTests)
endif()

# Marking these files as link dependencies makes the existing post-build copy
# run again even when no C++ source changed.
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS "${GAME_DATA_FILES}")

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if ("${PLATFORM}" STREQUAL "Web")
    # The browser receives every preloaded file up front. Stage only the files
    # used by the game instead of the full development asset library.
    set(WEB_ASSET_ROOT "${CMAKE_BINARY_DIR}/web_assets")
    file(MAKE_DIRECTORY "${WEB_ASSET_ROOT}/assets")
    file(COPY "${CMAKE_SOURCE_DIR}/game_data" DESTINATION "${WEB_ASSET_ROOT}")
    file(COPY "${CMAKE_SOURCE_DIR}/assets/Music" DESTINATION "${WEB_ASSET_ROOT}/assets")

    set(WEB_TEXTURE_ASSETS
        "assets/first_party/characters/Player_Sprites.png"
        "assets/first_party/characters/knight.png"
        "assets/first_party/characters/skull.png"
        "assets/first_party/environments/kiril-rock.png"
        "assets/first_party/environments/kiril-boulder.png"
        "assets/first_party/machines/chain_links.png"
        "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/1_Industrial_Tileset_1.png"
        "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/2_Industrial_Tileset_1_Background.png"
        "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/3_Far_Background_Tile.png"
        "assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/6. Character Animations 32p/Anim_Robot_Walk1_v1.1_spritesheet.png")
    foreach (WEB_TEXTURE_ASSET IN LISTS WEB_TEXTURE_ASSETS)
        get_filename_component(WEB_TEXTURE_DIRECTORY "${WEB_TEXTURE_ASSET}" DIRECTORY)
        file(MAKE_DIRECTORY "${WEB_ASSET_ROOT}/${WEB_TEXTURE_DIRECTORY}")
        file(COPY "${CMAKE_SOURCE_DIR}/${WEB_TEXTURE_ASSET}"
            DESTINATION "${WEB_ASSET_ROOT}/${WEB_TEXTURE_DIRECTORY}")
    endforeach()

    target_compile_definitions(RaylibGameCore PRIVATE GAME_WEB_BUILD=1)
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
    # Keep each Emscripten argument together. CMake de-duplicates individual
    # link options, which otherwise removes the second --preload-file flag and
    # makes game_data look like a linker input.
    target_link_options(${PROJECT_NAME} PUBLIC
        "SHELL:-sUSE_GLFW=3"
        # raylib's Web backend yields to the browser with emscripten_sleep().
        "SHELL:-sASYNCIFY"
        "SHELL:--preload-file ${WEB_ASSET_ROOT}@/")
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
