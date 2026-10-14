# Level editor

Build the `LevelEditor` CMake target and launch `build/LevelEditor/LevelEditor.exe`.

## Script attachments

Select a game object or one tile, then use **Objects > Attach Lua Script...**
or **Attach C++ Script...** to choose source files. The **Scripts** collection
in Details lets you edit or remove references. Tiles can also be selected from
the Tiles section of the outliner. Attachments belong to individual instances,
and participate in save/reopen and undo/redo. Paths are relative to the level
folder when the level already has a filename; Save As rebases those paths.

Level files store `objectScripts <record-type> <one-based-index> <JSON-path-array>`.
The game loader and native editor retain these references. Lua files run when
you press **Play / F5**, and reload when you restart playtesting or reset the
level. Temporary playtests resolve paths from the original level folder.
C++ files run as compiled plugins in the Windows desktop game. See the build
workflow below.

Lua scripts return a table containing any of these optional callbacks:
`on_start(self)`, `on_update(self, dt)`, `on_touch(self, playerIndex)`, and
`on_leave(self, playerIndex)`. Touch/leave fire on overlap entry/exit, once per
active player. Each attached file gets independent state per object instance;
store your own values on `self`. Pausing freezes update and overlap callbacks.

The initial API exposes `self.type`, `self.index`, and these methods:

| Method | Behavior |
| --- | --- |
| `self:get_bounds()` | Current world bounds as `{x, y, width, height}`. |
| `self:set_position(x, y)` | Move the attached instance's bounds to this top-left position. |
| `self:get_player(index)` | Player bounds, `vx`, `vy`, `on_ground`; `nil` for inactive players. Defaults to player 1. |
| `self:set_player_velocity(index, vx, vy)` | Set an active player's velocity. |
| `self:teleport_player(index, x, y)` | Move an active player. |
| `self:overlaps_player(index)` | Check overlap with the attached instance. Defaults to player 1. |
| `self:open_exit()` | Unlock and raise the level exit; the player must still enter it. |
| `self:log(message)` / `print(...)` | Write to the game's developer console (backtick key). |

Bounds/position/touch support individual visual tiles, solids, platforms,
ladders, enemies, blocks, wheels, gears, buttons, common machine parts and
guide objects. Connection records and tile rectangles do not expose spatial
operations; use individual tiles for scripted triggers. Scripts use the base,
math, string, table and UTF-8 libraries. Filesystem, shell, native modules,
debugging, metatables, dynamic loading and nested protected calls are omitted.
Each instance has an 8 MB allocation limit and a 100,000-instruction budget per
callback. Errors disable the affected file and report its path and target in
the developer console; other scripts continue.

Try attaching `game_data/scripts/bounce.lua` to a tile or
`game_data/scripts/open_exit_on_touch.lua` to an object. No game rebuild is
needed after editing these files: restart Play to load your changes.

## Building C++ scripts

1. Copy `game_data/scripts/bounce.cpp` as a starting point and edit its callbacks.
   Include `CppScript.h` and export `ppp_get_script` using `PPP_SCRIPT_EXPORT`.
2. Select a tile or object and use **Objects > Attach C++ Script...**.
3. Click **Build Scripts** on the toolbar, or use **Play > Build Scripts**.
   The compiler diagnostics window shows the result and any errors.
4. Press **Play / F5**. Play automatically rebuilds all current C++ attachments
   before launching; a failed build blocks playtesting. Edit, then restart Play
   to use the new behavior. Lua attachments can coexist with C++ on an instance.

Build Scripts uses the configured game compiler, a compiler on PATH, or
`C:\raylib\w64devkit\bin\g++.exe`. If none is available, it asks you to locate a
64-bit MinGW `g++.exe`. Set `PPP_CPP_COMPILER` to a full compiler path to override
automatic detection. MSVC and 32-bit compilers are not supported by this workflow.
The build ships `script_sdk/CppScript.h` beside the editor. No engine headers or
game rebuild are needed for script edits.

Each attached source file builds independently into `<source>.dll` beside that
source, for example `bounce.cpp.dll`. Keep the DLL with the source when sharing
a native level. Build output is replaced only after successful compilation.
An active playtest is stopped before a manual build, releasing its loaded DLLs.
The button changes to **Cancel Build** while compiling; the editor stays responsive.

The versioned SDK exposes bounds and position, player state/velocity/teleport,
overlap checks, exit unlocking, and console logging. `PppScript` declares optional
`create`, `destroy`, `on_start`, `on_update`, `on_touch`, and `on_leave` callbacks.
Allocate state in `create`, free it in `destroy`, and keep allocations within the
plugin. One state is created per attachment, even when several instances share a
DLL. Callback player indices start at 1. Host operations return 0 when the target
or player is unavailable. Read the complete contract in `script_sdk/CppScript.h`.

Native callbacks must not throw exceptions across the plugin boundary. C++ runs
inside the game process and can crash or stall it; Lua's memory/instruction limits
do not apply to C++. Native DLL execution is Windows-only. Lua continues to work
on the other game targets. Missing or incompatible DLLs are reported in the
developer console.

## Sprite sheets

The sprite sheet preview stays directly below its selector and shrinks to fit
small images. Layers sits immediately below it and scrolls independently, so
enlarging its list keeps the preview visible. Large sheets scroll within a
height-limited preview.
Use the folder button beside the selector or **Tile > Load Sprite Sheet...** to
import PNG, JPEG, BMP, GIF, TIFF, or ICO artwork. Sheets use the existing 32-pixel
tile grid; animated image files import their current frame as a static sheet.
The editor copies imported artwork as PNG into `assets/imported_sprite_sheets`
beside the editor. Level files save the image's full path and its sheet ID, so
painting, reopening, undo/redo, and playtesting use the same artwork. Keep those
imported files with the editor; moving them requires updating their level paths.

The transform toolbar has two targets: **Tile brush** rotates or flips artwork
for future painting, updating the sheet selection preview and viewport cursor;
**Selected tiles** rotates, flips, or duplicates tiles selected in the current
layer with the Tile Select tool. Duplicate places the entire copy beside the
original selection. A transform without a selection activates Tile Select and
shows instructions. The Tile menu also provides brush and selection transforms.

## Layers

Use **Move Up** and **Move Down** beside Add Layer and Delete Layer to reorder
custom layers. Higher custom layers draw in front of lower ones. Double-click a
custom layer's name to edit it in place; press Enter to finish or Escape to cancel.
Moving the top custom layer up skips the fixed default block and places it above
Foreground. Moving the bottom custom layer above that block down returns it below
the defaults. Middleground stays between Foreground and Background.
Names and order are saved with the level and participate in undo/redo. The
default layers cannot be renamed, reordered, or deleted.

Click the lock icon on any layer to toggle its editing lock. Closed locks mean
locked, and open locks mean unlocked. Each layer locks independently; its
visibility checkbox continues to control whether it is drawn.

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

## Object placement

The five default layers are **Foreground**, **Background**, **Far Background**,
**Collision**, and **Middleground**. Middleground controls visibility of gameplay
objects, including the player, and can also contain painted tiles. Hiding it
hides object selection and connection overlays; locking it prevents viewport
object manipulation. Default layers cannot be renamed or removed. Existing user
layers keep their saved identifiers.

The asset browser and **Object > Place** menu include the full advanced object
catalog: machinery, power objects, sensors, hazards, oil/mud regions, and pickups.
Click the viewport or drag a browser entry onto it to place an object. Edit its
authored settings in Details. Placement, movement, deletion, and properties use
the shared undo history and are saved with the level.

Water pits and toxin leaks use dragged bounds. A toxin leak creates an empty gas
field and its source together; edit the `toxinLeak` record in the outliner to
configure its emission. Water pit, toxin leak, clock face, and basic spike hazard
records are singletons: placing another updates the existing setup. Mounted Gear
places a gear with Mounted selected in its properties.

## Connections

Use **Object > Add Platform Loop...** to create a loop with its center, path
size, platform size/count, and speed. New loops run continuously; select a button
and use **Link...** to make that button activate a loop instead.

Use **Object > Attach Riding Button...** to choose a loop and platform number.
Selecting an existing button first attaches that button and preserves what it
controls; otherwise the tool creates a new button. Select a riding button and
use **Link...** to choose its action. Loop paths appear in Connections, and their
platforms and riding buttons appear in the viewport.

Edit loop and riding-link records in Details. Removing a riding link detaches
the button. Removing a loop's activation link makes it run continuously again.
**Object > Delete Selected Loop** removes the loop and its riding links while
keeping the buttons and their other control links. All operations support undo.

Select a loop by clicking one of its platforms, its path with Connections shown,
or its outliner entry. Use **Move** to drag it, **Scale** to resize its path, and
**Ctrl+D** to duplicate the loop with its riding buttons. Platforms keep their
sizes when resizing the path; **Alt** resizes around the center. Attached buttons
follow their platforms. New copies of riding buttons can be linked separately.

Select a **Button** or **Valve** in the viewport or outliner and click **Link**
on the toolbar (also under **View > Connections**), then click its target in the
viewport. A preview line follows the cursor; compatible targets get an outline
and a green line. **Escape**, right-click, or clicking **Link** again cancels.
Empty or incompatible clicks keep linking active without changing the level.
**View > Connections > Choose Target from List...** provides the dropdown option.
With Connections shown and Select active, click the middle of a purple control
line to select its saved link. The line highlights and Details shows its settings.
Press **Delete** to unlink it or **Escape** to clear selection. Endpoint clicks
remain available for selecting objects. Selecting a loop activation line and
deleting it leaves the loop running independently; selecting the loop object and
deleting it removes the loop. Power-channel and attachment overlays continue to
be edited through their objects' properties.
Buttons control trap doors, fans, exits, ladders,
platforms, and spikes; valves fill fluid regions. Purple lines show these controls.
Button-controlled geometry appears as outlined bounds in the connection overlay.

Select the new link record in the outliner to edit its settings in Details.
Choose **Remove Selected Link** or press **Delete** with that record selected to
unlink it. Unlinking ladders, platforms, and spikes restores ordinary geometry.
Creating and removing links participates in undo/redo and saves with the level.
The game uses one button-controlled exit and one valve fill setup per level;
linking those again replaces the previous link.

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

- `ctest --test-dir build -R "loop_editor|editor_placement|editor_connections|spring_physics|generator_physics" --output-on-failure`
- `dotnet run --project tests/editor_history/EditorHistoryTests.csproj`
- `dotnet run --project tests/editor_startup/EditorStartupTests.csproj` verifies that the default sprite sheet loads without dropdown interaction.

Tile passage: Select one or more tiles in the sprite sheet, then click Passage on
 the tile tools bar. Choose Passable, Solid, or check the blocked artwork sides.
Top-only creates a platform the player can jump through from below. Settings apply
to every placed copy of that source tile in the current level and follow rotation
and flips. Red edges in the palette mark blocked faces; a green circle marks an
explicit passable tile. Use layer default removes the override. Without an override,
Collision-layer tiles remain solid and other layers remain passable. Changes support
undo/redo and are saved with the level. Solid and directional tile collision
matches the body's Background, Middleground, or Foreground layer; players are on
Middleground. Collision-layer tiles affect every world layer. Far Background and
user layers have no matching physics bodies. The outlined C button beside each
layer's lock independently toggles its viewport collision lines. Orange lines
show blocked artwork faces; red lines show universal Collision-layer faces.
Overlay visibility does not change gameplay collision.

### Level and object audio

Use **File > Choose Level Music** to import WAV, OGG, MP3 or FLAC music. The track loops during gameplay and replaces built-in level music. **File > Remove Level Music** restores the default soundtrack.

Select a game object and use **Objects > Attach Sound Effect**. The sound plays once when a player enters the object's bounds and can play again after leaving and returning. Clear **Audio > Sound** in Details to remove it. Music and effects respect the game's volume and mute settings.

Import saves audio in an `audio` folder beside the level. Keep that folder with the level when sharing it. Audio attachments support undo, saving and temporary playtests.
