# Level editor

Build the `LevelEditor` CMake target and launch `build/LevelEditor/LevelEditor.exe`.

## Playtesting

Use **Play** or **F5** to launch the current document, including unsaved changes,
at its player start. Press **F5** in the game to return, or use **Stop** in the
editor. Playtests use a temporary level copy. Completing the level returns to
editing. The editor viewport stays open throughout testing.

## Undo and redo

**Ctrl+Z** and **Ctrl+Y** share a chronological history of document properties,
tiles, collision, and object edits. Each painting stroke or object drag is one
step. Saving retains history. Opening a different level resets it. The most
recent 100 edits are retained.

## Connections

Toggle **Connections** on the viewport toolbar, or use **View > Connections**.
Gold lines show membership in a shared nonzero power channel, green lines show
spring/rod/joint attachment targets, and blue lines show pulley links and
movement paths. Channel 0 objects operate independently. Red markers identify
missing power sources, attachment targets, or pulley anchors. These overlays
describe authored connections rather than live simulated power output.

With **Focus on Selected Network** enabled, selecting an eligible object shows
its connected component. Disable it to inspect every network. **Alt+click** a
connection endpoint to select that object. **Select Connected** or
**Ctrl+Shift+L** selects the whole connected component and highlights its objects
in the outliner.

Hover a node for its name and any warning. Change electrical objects' **Channel**
in Details to assign their network. Spring, rod, and fixed-joint targets match
the game's nearest eligible guide body within 42 world pixels on the same world
layer; the selected constraint shows that search radius. Movement paths show
the authored travel range or pendulum swing arc.

Regression checks:

- `ctest --test-dir build -R "editor_connections|spring_physics|generator_physics" --output-on-failure`
- `dotnet run --project tests/editor_history/EditorHistoryTests.csproj`
