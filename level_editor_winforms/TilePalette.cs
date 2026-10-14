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
    private int quarterTurns;
    private bool flipX;
    private bool flipY;

    internal event Action<int, int, int, int>? TileSelected;
    internal event Action? SheetSizeChanged;
    internal Func<int, int, int?>? PassageAt;
    internal Size SheetSize => sheet?.Size ?? Size.Empty;

    internal TilePalette()
    {
        DoubleBuffered = true;
        AutoScroll = true;
        Dock = DockStyle.Fill;
        BackColor = DarkTheme.Field;
    }

    internal void LoadSheet(string path)
    {
        // Clone the decoded bitmap so the source file remains free to replace or move.
        Bitmap? replacement = null;
        if (File.Exists(path))
        {
            using var image = new Bitmap(path);
            replacement = new Bitmap(image);
        }
        sheet?.Dispose();
        sheet = replacement;
        AutoScrollPosition = Point.Empty;
        AutoScrollMinSize = sheet?.Size ?? Size.Empty;
        selectedColumn = 0;
        selectedRow = 0;
        selectionWidth = 1;
        selectionHeight = 1;
        Invalidate();
        SheetSizeChanged?.Invoke();
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

    internal void SetBrushTransform(int turns, bool horizontalFlip, bool verticalFlip)
    {
        quarterTurns = turns;
        flipX = horizontalFlip;
        flipY = verticalFlip;
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
        if (PassageAt != null)
        {
            using var facePen = new Pen(Color.OrangeRed, 3);
            using var textBrush = new SolidBrush(Color.LimeGreen);
            for (var row = 0; row * 32 < sheet.Height; ++row)
            for (var column = 0; column * 32 < sheet.Width; ++column)
            {
                var mask = PassageAt(column, row);
                if (!mask.HasValue) continue;
                var x = offset.X + column * 32;
                var y = offset.Y + row * 32;
                if (mask == 0) eventArgs.Graphics.DrawString("○", Font, textBrush, x + 8, y + 8);
                if ((mask & 1) != 0) eventArgs.Graphics.DrawLine(facePen, x + 4, y + 3, x + 28, y + 3);
                if ((mask & 2) != 0) eventArgs.Graphics.DrawLine(facePen, x + 29, y + 4, x + 29, y + 28);
                if ((mask & 4) != 0) eventArgs.Graphics.DrawLine(facePen, x + 4, y + 29, x + 28, y + 29);
                if ((mask & 8) != 0) eventArgs.Graphics.DrawLine(facePen, x + 3, y + 4, x + 3, y + 28);
            }
        }

        if (quarterTurns != 0 || flipX || flipY)
        {
            for (var row = selectedRow; row < selectedRow + selectionHeight; ++row)
            for (var column = selectedColumn; column < selectedColumn + selectionWidth; ++column)
            {
                var state = eventArgs.Graphics.Save();
                var cell = new Rectangle(offset.X + column * 32, offset.Y + row * 32, 32, 32);
                eventArgs.Graphics.SetClip(cell);
                using var background = new SolidBrush(BackColor);
                eventArgs.Graphics.FillRectangle(background, cell);
                eventArgs.Graphics.TranslateTransform(cell.X + 16, cell.Y + 16);
                eventArgs.Graphics.RotateTransform(quarterTurns * 90);
                eventArgs.Graphics.ScaleTransform(flipX ? -1 : 1, flipY ? -1 : 1);
                eventArgs.Graphics.DrawImage(sheet, new Rectangle(-16, -16, 32, 32),
                    new Rectangle(column * 32, row * 32, 32, 32), GraphicsUnit.Pixel);
                eventArgs.Graphics.Restore(state);
            }
        }

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
