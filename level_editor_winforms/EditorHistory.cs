namespace PowerPulleyPanic.LevelEditor;

// Whole-document states preserve references, comments, settings, and objects
// that the viewport does not itself know how to edit.
internal sealed class EditorHistory
{
    private readonly List<string> states = [];
    private int position;
    private string savedState = "";
    internal bool IsDirty => states.Count > 0 && states[position] != savedState;

    internal void Reset(string state)
    {
        states.Clear();
        states.Add(state);
        position = 0;
        savedState = state;
    }

    internal void MarkSaved(string state) => savedState = state;

    internal void Record(string state)
    {
        if (states.Count == 0) { Reset(state); return; }
        if (states[position] == state) return;
        states.RemoveRange(position + 1, states.Count - position - 1);
        states.Add(state);
        position++;
        if (states.Count > 101) { states.RemoveAt(0); position--; }
    }

    internal string? Undo() => position > 0 ? states[--position] : null;
    internal string? Redo() => position + 1 < states.Count ? states[++position] : null;
}
