#include "TileSelectionTransform.h"
#include <stdexcept>
#include <iostream>

void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

int main() {
    VisualTile tile{TileLayer::User1, 3, 2, {64, 96}};
    tile.sheetIndex = 3;
    const VisualTile original = tile;
    // A 2 by 3 rectangle rotates into a 3 by 2 rectangle and back.
    TransformSelectedTile(tile, 5, 2, 3, 3, 5);
    Check(tile.position.x == 128 && tile.position.y == 96 && tile.quarterTurns == 1, "Clockwise rotation must move the corner and rotate its artwork.");
    TransformSelectedTile(tile, 4, 2, 3, 4, 4);
    Check(tile.position.x == original.position.x && tile.position.y == original.position.y && tile.quarterTurns == 0,
        "Inverse rotations must restore the tile.");
    TransformSelectedTile(tile, 6, 2, 3, 3, 5);
    Check(tile.position.x == 96 && tile.flipX, "Horizontal flip must mirror the cell and artwork.");
    TransformSelectedTile(tile, 6, 2, 3, 3, 5);
    TransformSelectedTile(tile, 7, 2, 3, 3, 5);
    Check(tile.position.y == 160 && tile.flipY, "Vertical flip must mirror the cell and artwork.");
    TransformSelectedTile(tile, 7, 2, 3, 3, 5);
    TransformSelectedTile(tile, 8, 2, 3, 3, 5);
    Check(tile.position.x == 128 && tile.position.y == 96, "A duplicate must lie beyond the entire source rectangle.");
    Check(tile.sheetIndex == 3 && tile.layer == TileLayer::User1 && tile.column == 3 && tile.row == 2,
        "Transforms must preserve custom artwork and its current layer.");
    std::cout << "Tile selection transform checks passed.\n";
}
