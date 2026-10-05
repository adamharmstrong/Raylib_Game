namespace PowerPulleyPanic.LevelEditor;

internal sealed class TilePalette : ScrollableControl
{
    private Bitmap? sheet;
    private int selectedColumn;
    private int selectedRow;
    private int selectionWidth = 1;
    private int selectionHeight = 1;
    private int dragColumn;
    private int dragRow;
    private bool selecting;

    internal event Action<int, int, int, int>? TileSelected;

    internal TilePalette()
    {
        DoubleBuffered = true;
        AutoScroll = true;
        Dock = DockStyle.Fill;
        BackColor = DarkTheme.Field;
    }

    internal void LoadSheet(string path)
    {
        sheet?.Dispose();
        sheet = File.Exists(path) ? new Bitmap(path) : null;
        AutoScrollMinSize = sheet?.Size ?? Size.Empty;
        selectedColumn = 0;
        selectedRow = 0;
        selectionWidth = 1;
        selectionHeight = 1;
        Invalidate();
        TileSelected?.Invoke(0, 0, 1, 1);
    }

    internal void SelectTile(int column, int row)
    {
        if (sheet == null || column < 0 || row < 0 || column * 32 >= sheet.Width || row * 32 >= sheet.Height) return;
        selectedColumn = column;
        selectedRow = row;
        selectionWidth = 1;
        selectionHeight = 1;
        AutoScrollPosition = new Point(Math.Max(0, column * 32 - ClientSize.Width / 2),
            Math.Max(0, row * 32 - ClientSize.Height / 2));
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs eventArgs)
    {
        base.OnPaint(eventArgs);
        if (sheet == null) return;
        var offset = AutoScrollPosition;
        eventArgs.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
        eventArgs.Graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
        eventArgs.Graphics.DrawImageUnscaled(sheet, offset.X, offset.Y);

        using var grid = new Pen(Color.FromArgb(95, 255, 255, 255));
        for (var x = 0; x <= sheet.Width; x += 32)
            eventArgs.Graphics.DrawLine(grid, offset.X + x, offset.Y, offset.X + x, offset.Y + sheet.Height);
        for (var y = 0; y <= sheet.Height; y += 32)
            eventArgs.Graphics.DrawLine(grid, offset.X, offset.Y + y, offset.X + sheet.Width, offset.Y + y);

        using var fill = new SolidBrush(Color.FromArgb(55, DarkTheme.Accent));
        using var selected = new Pen(DarkTheme.Accent, 3.0f);
        var bounds = new Rectangle(offset.X + selectedColumn * 32 + 1, offset.Y + selectedRow * 32 + 1,
            selectionWidth * 32 - 2, selectionHeight * 32 - 2);
        eventArgs.Graphics.FillRectangle(fill, bounds);
        eventArgs.Graphics.DrawRectangle(selected, bounds);
    }

    protected override void OnMouseDown(MouseEventArgs eventArgs)
    {
        base.OnMouseDown(eventArgs);
        if (sheet == null || eventArgs.Button != MouseButtons.Left) return;
        var x = eventArgs.X - AutoScrollPosition.X;
        var y = eventArgs.Y - AutoScrollPosition.Y;
        if (x < 0 || y < 0 || x >= sheet.Width || y >= sheet.Height) return;
        selecting = true;
        Capture = true;
        dragColumn = x / 32;
        dragRow = y / 32;
        UpdateSelection(dragColumn, dragRow);
    }

    protected override void OnMouseMove(MouseEventArgs eventArgs)
    {
        base.OnMouseMove(eventArgs);
        if (!selecting || sheet == null) return;
        var x = Math.Clamp(eventArgs.X - AutoScrollPosition.X, 0, sheet.Width - 1);
        var y = Math.Clamp(eventArgs.Y - AutoScrollPosition.Y, 0, sheet.Height - 1);
        UpdateSelection(x / 32, y / 32);
    }

    protected override void OnMouseUp(MouseEventArgs eventArgs)
    {
        base.OnMouseUp(eventArgs);
        if (eventArgs.Button != MouseButtons.Left) return;
        selecting = false;
        Capture = false;
    }

    private void UpdateSelection(int column, int row)
    {
        selectedColumn = Math.Min(dragColumn, column);
        selectedRow = Math.Min(dragRow, row);
        selectionWidth = Math.Abs(column - dragColumn) + 1;
        selectionHeight = Math.Abs(row - dragRow) + 1;
        Invalidate();
        TileSelected?.Invoke(selectedColumn, selectedRow, selectionWidth, selectionHeight);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) sheet?.Dispose();
        base.Dispose(disposing);
    }
}
