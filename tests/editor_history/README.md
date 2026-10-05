Run `dotnet run --project tests/editor_history/EditorHistoryTests.csproj` from the repository root.

These regression tests exercise the editor's shared document history, including
mixed property/painting/object changes, grouping a paint stroke, undo across
saves, redo branching, snapshot preservation, resetting for another level, and
the 100-edit limit. They run without opening the game or editor.
