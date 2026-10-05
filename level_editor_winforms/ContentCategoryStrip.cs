using System.Drawing.Drawing2D;
using System.ComponentModel;

namespace PowerPulleyPanic.LevelEditor;

internal sealed class ContentCategoryStrip : Control
{
    private readonly List<Rectangle> tabBounds = [];
    private string selectedCategory = "All";
    private const int TabHeight = 28;

    internal List<string> Categories { get; } = [];

    [Browsable(false)]
    [DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
    internal string SelectedCategory
    {
        get => selectedCategory;
        set
        {
            if (selectedCategory == value) return;
            selectedCategory = value;
            Invalidate();
            SelectedCategoryChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    internal event EventHandler? SelectedCategoryChanged;

    internal ContentCategoryStrip()
    {
        Location = Point.Empty;
        Height = TabHeight;
        BackColor = DarkTheme.Field;
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw |
                 ControlStyles.UserPaint, true);
    }

    internal void RefreshLayout()
    {
        tabBounds.Clear();
        var x = 0;
        var y = 0;
        foreach (var category in Categories)
        {
            var textWidth = TextRenderer.MeasureText(category, Font,
                Size.Empty, TextFormatFlags.NoPadding).Width;
            var width = textWidth + 30;
            if (x > 0 && x + width > ClientSize.Width)
            {
                x = 0;
                y += TabHeight;
            }

            tabBounds.Add(new Rectangle(x, y, width, TabHeight - 1));
            x += width + 2;
        }

        var requiredHeight = Math.Max(TabHeight, y + TabHeight);
        if (Height != requiredHeight) Height = requiredHeight;
        Invalidate();
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        if (Categories.Count > 0) RefreshLayout();
    }

    protected override void OnFontChanged(EventArgs e)
    {
        base.OnFontChanged(e);
        if (Categories.Count > 0) RefreshLayout();
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        for (var index = 0; index < tabBounds.Count; ++index)
        {
            if (!tabBounds[index].Contains(e.Location)) continue;
            SelectedCategory = Categories[index];
            return;
        }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(DarkTheme.Field);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        for (var index = 0; index < tabBounds.Count; ++index)
        {
            var bounds = tabBounds[index];
            var selected = Categories[index] == SelectedCategory;
            using var path = CreateTabPath(bounds);
            using var fill = new SolidBrush(selected ? DarkTheme.Selection : DarkTheme.Header);
            using var border = new Pen(selected ? DarkTheme.Accent : DarkTheme.Border, selected ? 1.5f : 1f);
            e.Graphics.FillPath(fill, path);
            e.Graphics.DrawPath(border, path);

            var textBounds = new Rectangle(bounds.X + 7, bounds.Y + 1,
                Math.Max(1, bounds.Width - 20), bounds.Height - 2);
            TextRenderer.DrawText(e.Graphics, Categories[index], Font, textBounds,
                selected ? Color.White : DarkTheme.Text,
                TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter |
                TextFormatFlags.NoPadding | TextFormatFlags.SingleLine);
        }
    }

    private static GraphicsPath CreateTabPath(Rectangle bounds)
    {
        const int radius = 7;
        const int slant = 10;
        var right = bounds.Right - 1;
        var bottom = bounds.Bottom - 1;
        var path = new GraphicsPath();
        path.StartFigure();
        path.AddLine(bounds.X, bottom, bounds.X, bounds.Y + radius);
        path.AddArc(bounds.X, bounds.Y, radius * 2, radius * 2, 180, 90);
        path.AddLine(bounds.X + radius, bounds.Y, right - slant - radius, bounds.Y);
        path.AddBezier(right - slant - radius, bounds.Y, right - slant - 3, bounds.Y,
            right - slant, bounds.Y + 3, right - slant, bounds.Y + radius);
        path.AddLine(right - slant, bounds.Y + radius, right, bottom);
        path.AddLine(right, bottom, bounds.X, bottom);
        path.CloseFigure();
        return path;
    }
}
