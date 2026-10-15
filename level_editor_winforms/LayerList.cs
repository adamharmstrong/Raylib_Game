using System.Drawing.Drawing2D;
using System.ComponentModel;
using System.Windows.Forms.VisualStyles;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class LayerList : ListBox
{
    private readonly HashSet<object> checkedItems = [];
    private TextBox? renameEditor;
    private object? renamedItem;
    internal bool IsRenaming => renameEditor != null;
    internal event ItemCheckEventHandler? ItemCheck;
    internal event Action<int>? LockClicked;
    internal event Action<int>? CollisionClicked;
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal Func<int, bool>? ShowsCollision { get; set; }
    private readonly ToolTip glyphTip = new();
    private string glyphTipText = string.Empty;
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal Func<int, bool>? IsLocked { get; set; }
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal Func<int, bool>? CanRename { get; set; }
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal Func<int, string, bool>? RenameRequested { get; set; }

    internal LayerList()
    {
        DrawMode = DrawMode.OwnerDrawFixed;
        IntegralHeight = false;
        ItemHeight = Font.Height + 8;
        SetStyle(ControlStyles.ResizeRedraw | ControlStyles.OptimizedDoubleBuffer, true);
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        Invalidate();
    }

    internal bool GetItemChecked(int index) => checkedItems.Contains(Items[index]);
    internal void ClearCheckStates() { CancelRename(); checkedItems.Clear(); }
    internal void SetItemChecked(int index, bool value)
    {
        var previous = GetItemChecked(index);
        if (previous == value) return;
        var args = new ItemCheckEventArgs(index, value ? CheckState.Checked : CheckState.Unchecked,
            previous ? CheckState.Checked : CheckState.Unchecked);
        ItemCheck?.Invoke(this, args);
        if (args.NewValue == CheckState.Checked) checkedItems.Add(Items[index]);
        else checkedItems.Remove(Items[index]);
        Invalidate(GetItemRectangle(index));
    }

    private int GlyphWidth => Math.Max(24, (int)(24 * DeviceDpi / 96f));
    private Rectangle NameBounds(int index)
    {
        var row = GetItemRectangle(index);
        return new Rectangle(row.Left + GlyphWidth, row.Top, Math.Max(1, row.Width - GlyphWidth * 3), row.Height);
    }

    protected override void OnFontChanged(EventArgs e)
    {
        base.OnFontChanged(e);
        ItemHeight = Font.Height + 8;
    }

    protected override void OnDrawItem(DrawItemEventArgs e)
    {
        if (e.Index < 0 || e.Index >= Items.Count) return;
        var selected = (e.State & DrawItemState.Selected) != 0;
        using var background = new SolidBrush(selected ? DarkTheme.Selection : BackColor);
        e.Graphics.FillRectangle(background, e.Bounds);
        var checkState = GetItemChecked(e.Index) ? CheckBoxState.CheckedNormal : CheckBoxState.UncheckedNormal;
        var glyph = CheckBoxRenderer.GetGlyphSize(e.Graphics, checkState);
        CheckBoxRenderer.DrawCheckBox(e.Graphics,
            new Point(e.Bounds.Left + (GlyphWidth - glyph.Width) / 2, e.Bounds.Top + (e.Bounds.Height - glyph.Height) / 2), checkState);
        TextRenderer.DrawText(e.Graphics, Items[e.Index].ToString(), Font, NameBounds(e.Index), DarkTheme.Text,
            TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix | TextFormatFlags.SingleLine);
        var locked = IsLocked?.Invoke(e.Index) ?? false;
        var collisionBounds = new Rectangle(e.Bounds.Right - GlyphWidth * 2 + 4, e.Bounds.Top + 4,
            GlyphWidth - 8, e.Bounds.Height - 8);
        var rowColor = selected ? DarkTheme.Selection : BackColor;
        var collisionColor = ShowsCollision?.Invoke(e.Index) == true ? Color.Orange :
            Color.FromArgb(Math.Min(255, rowColor.R + 12), Math.Min(255, rowColor.G + 12), Math.Min(255, rowColor.B + 12));
        using (var collisionPen = new Pen(collisionColor, 2))
            e.Graphics.DrawRectangle(collisionPen, collisionBounds);
        TextRenderer.DrawText(e.Graphics, "C", Font, collisionBounds,
            collisionColor,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        var state = e.Graphics.Save();
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var scale = DeviceDpi / 96f;
        e.Graphics.TranslateTransform(e.Bounds.Right - GlyphWidth + (GlyphWidth - 16 * scale) / 2,
            e.Bounds.Top + (e.Bounds.Height - 18 * scale) / 2);
        e.Graphics.ScaleTransform(scale, scale);
        using var pen = new Pen(locked ? DarkTheme.Accent : DarkTheme.Text, 1.8f);
        e.Graphics.DrawRectangle(pen, 2, 8, 12, 9);
        if (locked) e.Graphics.DrawArc(pen, 5, 1, 6, 12, 180, 180);
        else
        {
            e.Graphics.DrawArc(pen, 7, -2, 8, 10, 180, 180);
            e.Graphics.DrawLine(pen, 7, 3, 7, 8);
        }
        e.Graphics.DrawLine(pen, 8, 11, 8, 14);
        e.Graphics.Restore(state);
        if (selected) e.DrawFocusRectangle();
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Left) return;
        var index = IndexFromPoint(e.Location);
        if (index < 0) return;
        var row = GetItemRectangle(index);
        if (e.X >= row.Right - GlyphWidth) LockClicked?.Invoke(index);
        else if (e.X >= row.Right - GlyphWidth * 2) CollisionClicked?.Invoke(index);
        else if (e.X < GlyphWidth) SetItemChecked(index, !GetItemChecked(index));
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        var index = IndexFromPoint(e.Location);
        var text = string.Empty;
        if (index >= 0)
        {
            var row = GetItemRectangle(index);
            text = e.X >= row.Right - GlyphWidth ? "Lock layer" :
                e.X >= row.Right - GlyphWidth * 2 ? "Show or hide tile collision lines" :
                e.X < GlyphWidth ? "Show or hide layer" : string.Empty;
        }
        if (text == glyphTipText) return;
        glyphTipText = text;
        glyphTip.SetToolTip(this, text);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) glyphTip.Dispose();
        base.Dispose(disposing);
    }

    protected override void OnMouseDoubleClick(MouseEventArgs e)
    {
        base.OnMouseDoubleClick(e);
        var index = IndexFromPoint(e.Location);
        if (e.Button != MouseButtons.Left || index < 0 || !NameBounds(index).Contains(e.Location) || CanRename?.Invoke(index) != true) return;
        BeginRename(index);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (SelectedIndex >= 0 && e.KeyCode == Keys.Space) { SetItemChecked(SelectedIndex, !GetItemChecked(SelectedIndex)); e.Handled = true; }
        else if (SelectedIndex >= 0 && e.KeyCode == Keys.F2) { BeginRename(SelectedIndex); e.Handled = true; }
        else base.OnKeyDown(e);
    }

    internal void BeginRename(int index)
    {
        if (CanRename?.Invoke(index) != true) return;
        CancelRename();
        renamedItem = Items[index];
        renameEditor = new TextBox { Text = renamedItem.ToString(), Bounds = NameBounds(index), MaxLength = 80,
            BackColor = DarkTheme.Field, ForeColor = DarkTheme.Text, BorderStyle = BorderStyle.FixedSingle };
        renameEditor.KeyDown += (_, e) =>
        {
            if (e.KeyCode == Keys.Enter) { CommitRename(); e.SuppressKeyPress = true; }
            else if (e.KeyCode == Keys.Escape) { CancelRename(); e.SuppressKeyPress = true; }
        };
        renameEditor.LostFocus += (_, _) => CommitRename();
        Controls.Add(renameEditor);
        renameEditor.BringToFront();
        renameEditor.Focus();
        renameEditor.SelectAll();
    }

    private void CommitRename()
    {
        if (renameEditor == null || renamedItem == null) return;
        var index = Items.IndexOf(renamedItem);
        var text = renameEditor.Text;
        // Detach before refreshing the list to avoid committing again on LostFocus.
        var editor = renameEditor;
        renameEditor = null;
        renamedItem = null;
        if (index >= 0) RenameRequested?.Invoke(index, text);
        Controls.Remove(editor);
        editor.Dispose();
    }

    private void CancelRename()
    {
        var editor = renameEditor;
        renameEditor = null;
        renamedItem = null;
        if (editor == null) return;
        Controls.Remove(editor);
        editor.Dispose();
    }
}
