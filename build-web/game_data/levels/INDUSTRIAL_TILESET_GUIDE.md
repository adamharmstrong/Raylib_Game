# Industrial Tileset Authoring Guide

This guide defines how campaign and test levels should use the 32-pixel
industrial tileset. Its purpose is to keep new level art consistent with the
vendor examples and prevent visual tiles from drifting away from collision
geometry.

## Sources Of Truth

Use these files, in this order:

1. `game_data/levels/tileset_reference.level`
2. `assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/Target_Example.png`
3. `assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/Target_Example_2.png`
4. `assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/How To Part 1 - RESIZED.png`
5. `assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/How To Part 2 - RESIZED.png`
6. `assets/third_party/AtomicRealm/[FREE] Industrial Tileset/raw/FREE/5. Industrial Tileset - Starter Pack 32p/0_Template_Tileset.png`

The resized how-to images are at the same tile scale as the game sheets. The
reference level is a cell-by-cell transcription of the target example.

Do not use `Target_Example.psd` to infer layers. Its layers are not arranged
correctly. Also ignore the white arrows, white grid lines, player sprite,
vignette, and other presentation artifacts in the PNG examples.

The game loads these three sheets:

- `1_Industrial_Tileset_1.png`: foreground architecture
- `2_Industrial_Tileset_1_Background.png`: blue background architecture
- `3_Far_Background_Tile.png`: pale open-room field

The `1B`, `1C`, and violet variants are not selected by level records.

## The Three Visual Layers

The tileset is a three-layer composition, drawn back to front:

1. `farBackground`
2. `background`
3. `foreground`

These are visual layers. They are separate from the physics-object
`layer <background|middleground|foreground>` record described in `README.md`.
Visual tile layers choose artwork and draw order. Physics world layers choose
object depth, object-to-object contacts, and whether an object can collide with
the player.

### Far Background

The far-background sheet contains one pale 32x32 tile at `(0,0)`. Paint it
wherever the player should see open chamber space.

Once a level contains any `farBackground` tile, the renderer fills otherwise
unpainted world space with the foreground void tile `(1,2)`, which appears
black. This makes the far-background mask significant:

- Paint all visible open room space.
- Paint open pits when they should match the room background.
- Leave enclosed structural interiors unpainted when they should remain black.
- Leave space outside the room unpainted unless the level intentionally
  continues there.
- For multi-screen levels, cover the open area across the full world bounds,
  not only the first camera view.

Do not omit the lower part of a pit merely because it is below the floor. That
produces an unintended black rectangle.

### Background

The background sheet supplies blue structural depth behind the play plane. It
never supplies terrain collision.

In the vendor example, its grid is offset by 16 pixels in both axes from the
foreground grid. If the foreground origin is `(0,0)` modulo 32, background
clusters normally begin at coordinates congruent to `(16,16)` modulo 32. The
reference room demonstrates this with:

- foreground origin: `(528,144)`
- background origin: `(544,160)`
- far-background origin: `(560,176)`

The far-background origin is 32 pixels beyond the foreground origin, so it is
on the same modulo-32 grid.

Background structures should be deliberate clusters, recesses, or broad
silhouettes. Do not wallpaper the room with an edge tile. A common 3x3 panel is:

```text
visualTile background 0 0 80 80
visualTile background 1 0 112 80
visualTile background 2 0 144 80
visualTile background 0 1 80 112
visualTile background 1 1 112 112
visualTile background 2 1 144 112
visualTile background 0 3 80 144
visualTile background 1 3 112 144
visualTile background 2 3 144 144
```

For wider panels, repeat column `1` only in the middle of a row. Columns `0`
and `2` belong at the left and right edges.

### Foreground

The foreground sheet supplies floors, ceilings, walls, platforms, corners, and
junctions on the play plane. Foreground artwork should trace the boundary
between open room space and solid structural mass.

Do not fill a large solid rectangle with repeated floor tiles. Draw one
perimeter:

- one floor/top row
- correctly oriented side walls
- one ceiling/bottom row when the lower edge is visible
- black or otherwise intentional interior space

## Coordinates And Level Records

Both architecture sheets are 192x128 pixels, arranged as 6 columns by 4 rows
of 32x32 cells. Sheet coordinates are zero-based.

```text
visualTile <layer> <sheetColumn> <sheetRow> <worldX> <worldY>
visualTileRect <layer> <sheetColumn> <sheetRow> <worldX> <worldY> <columns> <rows>
```

`visualTileRect` repeats the same source cell at 32-pixel intervals. It does
not advance through the source sheet. It is appropriate for:

- the single far-background tile
- repeated center sections of a floor or ceiling
- repeated body sections of a wall

It is not a shortcut for a left-center-right strip or a top-middle-bottom
panel. Place those edge cells separately.

Use a 32-pixel grid by default. A complete structure may be translated off the
main grid to align with game mechanics, but every tile within that structure
must retain 32-pixel spacing. Never stretch a source tile.

## Foreground Atlas Vocabulary

The first four columns contain the reusable basic pieces.

| Structure | Left or top | Repeat or body | Right or bottom |
| --- | --- | --- | --- |
| Thin horizontal platform | `(0,0)` | `(1,0)` | `(2,0)` |
| Floor/top of a thick mass | `(0,1)` | `(1,1)` | `(2,1)` |
| Vertical sides of a thick mass | `(0,2)` | n/a | `(2,2)` |
| Ceiling/bottom of a thick mass | `(0,3)` | `(1,3)` | `(2,3)` |

Column `3` is the thin vertical strip:

| Role | Cell |
| --- | --- |
| Top cap | `(3,0)` |
| Upper/body segment | `(3,1)` |
| Lower/body segment | `(3,2)` |
| Bottom cap | `(3,3)` |

Foreground cell `(1,2)` is the black void cell. It is not a generic wall
center and must never be used as a floor. The renderer also uses this cell as
the base outside a far-background mask. In `tileset_reference`, it is
explicitly excluded from generated collision.

Columns `4` and `5` contain the special corner and junction atlas from the
irregular right-hand shape in `0_Template_Tileset.png`. Use these cells only
when the local boundary topology matches a known example. Common verified uses
include:

- `(4,0)` and `(5,0)` at the left and right ends of an exterior ceiling
- `(4,1)` and `(5,1)` where a floor meets an exterior side
- `(5,3)` where a vertical member branches downward from a ceiling
- other column `4` and `5` pieces at concave corners and multi-way junctions

For an uncommon junction, copy the matching arrangement from
`tileset_reference.level`. Do not guess based only on color or visual density.

## Boundary Rules

Choose tiles according to the side that touches open room space.

### Floors

A walkable floor is the upper boundary of solid mass:

- Use row `1` for the top of a thick foundation.
- Use row `0` for a free-standing platform that is only one tile high.
- Put a left cap at the beginning and a right cap at the end.
- Repeat only the center tile between the caps.
- Draw exactly one floor row.

Example for a three-tile platform:

```text
visualTile foreground 0 0 407 570
visualTile foreground 1 0 439 570
visualTile foreground 2 0 471 570
platform 407 570 96 32
```

### Ceilings

A ceiling is the lower boundary of solid mass above open room space:

- Use row `3`, not a floor row.
- Repeat `(1,3)` through a straight run.
- Use the correct corner or junction cell where the ceiling turns into a wall
  or sprouts a divider.

The top border of an enclosed level is visually a ceiling because the room is
below it. It should not be a repeated floor surface.

### Walls

Walls use vertical tiles, never rotated or stacked floor tiles.

- Use `(0,2)` when open room space is on the tile's left.
- Use `(2,2)` when open room space is on the tile's right.
- A left exterior screen wall therefore uses `(2,2)`.
- A right exterior screen wall therefore uses `(0,2)`.
- Use the column `3` strip for a thin divider, not for the edge of a thick
  foundation.

Alternate verified body variants only where the reference topology calls for
them. Do not alternate rows by accident.

### Corners And Intersections

At a turn or intersection, one tile owns the position:

- Do not place a floor tile and wall tile at the same layer and coordinates.
- Use the corresponding corner or junction cell.
- Do not allow two `visualTile` records to overlap on the same visual layer.
- A wall meeting an existing floor is not another floor row.
- A wall meeting another wall is not a platform cap.

When uncertain, find the same open/solid neighbor pattern in
`tileset_reference.level` and reuse that sheet coordinate.

### Thick Foundations

Draw the perimeter and leave the center alone. This example describes a
320x224 solid mass:

```text
# Top floor row: 10 cells.
visualTile foreground 0 1 320 640
visualTileRect foreground 1 1 352 640 8 1
visualTile foreground 2 1 608 640

# Five visible wall-body rows.
visualTileRect foreground 0 2 320 672 1 5
visualTileRect foreground 2 2 608 672 1 5

# Bottom ceiling row.
visualTile foreground 0 3 320 832
visualTileRect foreground 1 3 352 832 8 1
visualTile foreground 2 3 608 832

solid 320 640 320 224
```

Whether its interior appears pale or black is controlled by the far-background
mask behind it. For a solid black structural mass, omit far-background tiles
from the interior.

## Collision Rules

Visual tiles and terrain collision are intentionally separate in normal
levels.

- Use `solid` for static terrain and enclosed structural mass.
- Use `platform` for the collision rectangle of an explicitly tiled platform.
- Make the collision top exactly equal to the visible floor or platform top.
- Make walls and ceilings cover the same world coordinates as their visible
  tiles.
- A player is 40 pixels tall. Set `playerStart.y` so the player's feet equal
  the intended floor top when an exact grounded spawn is required.
- Background and far-background visual tiles never collide.
- Foreground visual tiles do not automatically collide in campaign levels.
- Terrain collision is shared across all physics world layers.
- Only middleground physics objects directly collide with the player.

`tileset_reference` is the sole current exception: `BuildSolids` derives 32x32
collision cells from its non-void foreground visual tiles. Do not assume that
behavior in another script.

Prefer one collision rectangle for a rectangular mass instead of one
`solid` per visual cell. Collision should describe the physical shape, while
the visual perimeter describes its surface.

## Renderer-Owned Structures

Some structures are generated by the renderer and collision builder rather
than by `visualTile` records:

- spike shaft side walls
- the spike row
- the foundation beneath a spike row
- dynamic water and other fluid fields
- ladders and most machinery

`spikeHazard` creates the rendered shaft walls and foundation. `BuildSolids`
also adds matching colliders. Use `spikePitTop` when a nonstandard level needs
to define the shaft's upper edge. Do not duplicate these walls or the
foundation with foreground records unless the renderer is changed at the same
time.

The far-background mask should still cover visible open space behind a pit and
fluid. Keep opaque foreground tiles out of space that animated water must
occupy.

## Recommended Authoring Order

1. Define `bounds`, camera zones, spawn, exits, hazards, and gameplay objects.
2. Draw the collision layout with `solid` and `platform`.
3. Mark every open chamber cell with `farBackground`.
4. Deliberately omit far-background from black void and solid interiors.
5. Add blue background structures on the 16-pixel-offset grid.
6. Trace the open/solid boundary with foreground floor, wall, ceiling, corner,
   and junction cells.
7. Remove duplicate visual positions.
8. Verify that every visual surface has matching collision and every collision
   surface has intentional artwork.
9. Run the level at native pixel scale and compare it with the reference PNGs.
10. Build the game and confirm the runtime copy of the level matches the source.

## Common Failure Modes

| Symptom | Likely cause | Correction |
| --- | --- | --- |
| Vertical seams repeat across open background | An edge tile was used as a center tile | Reserve columns `0` and `2` for edges; repeat column `1` |
| A foundation looks like several stacked floors | Floor rows were used as fill | Keep one top row, use wall sides and a bottom ceiling |
| Pit or lower room is black | Far-background mask stops at the floor | Extend `(0,0)` far-background tiles through visible pit space |
| Solid foundation interior is pale | Far background was painted behind the mass | Remove far-background cells from the enclosed interior |
| Wall looks horizontal | A row `0` or row `1` tile was stacked vertically | Use `(0,2)`, `(2,2)`, or the column `3` strip |
| Wall face points outward | Left/right wall orientation is reversed | Choose the wall whose detailed face borders open room space |
| Junction contains a small floor fragment | Two basic edges were overlaid or the wrong cap was used | Replace them with one matching column `4` or `5` junction |
| Floor collision feels sunken or floating | Collision top differs from visual tile top | Use the exact same `y` coordinate |
| Player is trapped by background art | Physics world layers were confused with visual layers | Keep background art visual-only; put player-plane objects in `middleground` |
| Hazard walls appear doubled | Renderer-owned pit tiles were also authored manually | Remove duplicate foreground pit walls/foundation |
| A repeated rectangle has identical edge pieces throughout | `visualTileRect` was expected to walk the atlas | Place edge cells explicitly; repeat only the center cell |

## Final Review Checklist

- [ ] Open room space is fully covered by far-background tiles.
- [ ] Intended void and solid interiors remain black.
- [ ] Background clusters are offset 16 pixels from the foreground grid.
- [ ] Background edge cells do not repeat through panel centers.
- [ ] Thin platforms use row `0` and are exactly one visual row high.
- [ ] Thick floors use one row `1` surface.
- [ ] Ceilings use row `3`.
- [ ] Walls use vertical cells and face the open room.
- [ ] Corners and intersections use one matching junction cell.
- [ ] No visual layer contains duplicate world positions.
- [ ] Normal levels have explicit `solid` or `platform` collision.
- [ ] Spawn feet, floor tops, platform tops, and exit bottoms align.
- [ ] Spike shafts and foundations are not duplicated.
- [ ] Dynamic fluid space is not covered by opaque foreground tiles.
- [ ] The level has been checked at native scale against the reference PNGs.

