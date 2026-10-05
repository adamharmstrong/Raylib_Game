namespace PowerPulleyPanic.LevelEditor;

internal static class DarkTheme
{
    internal static readonly Color Window = Color.FromArgb(25, 28, 34);
    internal static readonly Color Panel = Color.FromArgb(31, 35, 43);
    internal static readonly Color Header = Color.FromArgb(39, 44, 54);
    internal static readonly Color Field = Color.FromArgb(22, 25, 31);
    internal static readonly Color Border = Color.FromArgb(61, 68, 81);
    internal static readonly Color Text = Color.FromArgb(226, 231, 239);
    internal static readonly Color MutedText = Color.FromArgb(151, 160, 176);
    internal static readonly Color Accent = Color.FromArgb(69, 154, 255);
    internal static readonly Color Selection = Color.FromArgb(48, 91, 139);

    internal static void Apply(Form form)
    {
        form.BackColor = Window;
        form.ForeColor = Text;
        form.HandleCreated += (_, _) => ApplyWindowChrome(form);
        ApplyWindowChrome(form);
        ApplyControl(form);
    }

    private static void ApplyWindowChrome(Form form)
    {
        if (!form.IsHandleCreated) return;
        var enabled = 1;
        NativeMethods.DwmSetWindowAttribute(form.Handle, 20, ref enabled, sizeof(int));
    }

    private static void ApplyControl(Control control)
    {
        switch (control)
        {
            case Form:
                control.BackColor = Window;
                control.ForeColor = Text;
                break;
            case MenuStrip menu:
                menu.BackColor = Header;
                menu.ForeColor = Text;
                menu.Renderer = new ToolStripProfessionalRenderer(new DarkColorTable());
                ApplyToolStripItems(menu.Items);
                break;
            case StatusStrip status:
                status.BackColor = Header;
                status.ForeColor = MutedText;
                status.Renderer = new ToolStripProfessionalRenderer(new DarkColorTable());
                break;
            case ToolStrip strip:
                strip.BackColor = Header;
                strip.ForeColor = Text;
                strip.Renderer = new ToolStripProfessionalRenderer(new DarkColorTable());
                ApplyToolStripItems(strip.Items);
                break;
            case SplitContainer split:
                split.BackColor = Border;
                split.Panel1.BackColor = Panel;
                split.Panel2.BackColor = Panel;
                break;
            case TreeView tree:
                tree.BackColor = Field;
                tree.ForeColor = Text;
                tree.LineColor = MutedText;
                ApplyNativeDarkTheme(tree);
                break;
            case ListView list:
                list.BackColor = Field;
                list.ForeColor = Text;
                ApplyNativeDarkTheme(list);
                break;
            case PropertyGrid grid:
                grid.BackColor = Panel;
                grid.ViewBackColor = Field;
                grid.ViewForeColor = Text;
                grid.ViewBorderColor = Border;
                grid.CategoryForeColor = Text;
                grid.CategorySplitterColor = Border;
                grid.CommandsBackColor = Panel;
                grid.CommandsForeColor = Text;
                grid.HelpBackColor = Panel;
                grid.HelpForeColor = MutedText;
                grid.HelpBorderColor = Border;
                grid.LineColor = Border;
                ApplyNativeDarkTheme(grid);
                break;
            case Label label:
                label.BackColor = Header;
                label.ForeColor = Text;
                break;
            default:
                if (control is not ViewportHost)
                {
                    control.BackColor = Panel;
                    control.ForeColor = Text;
                }
                break;
        }

        foreach (Control child in control.Controls) ApplyControl(child);
    }

    private static void ApplyToolStripItems(ToolStripItemCollection items)
    {
        foreach (ToolStripItem item in items)
        {
            item.ForeColor = Text;
            if (item is ToolStripMenuItem menuItem) ApplyToolStripItems(menuItem.DropDownItems);
        }
    }

    private static void ApplyNativeDarkTheme(Control control)
    {
        control.HandleCreated += (_, _) => NativeMethods.SetWindowTheme(control.Handle, "DarkMode_Explorer", null);
        if (control.IsHandleCreated) NativeMethods.SetWindowTheme(control.Handle, "DarkMode_Explorer", null);
    }

    private sealed class DarkColorTable : ProfessionalColorTable
    {
        public override Color ToolStripGradientBegin => Header;
        public override Color ToolStripGradientMiddle => Header;
        public override Color ToolStripGradientEnd => Header;
        public override Color MenuStripGradientBegin => Header;
        public override Color MenuStripGradientEnd => Header;
        public override Color StatusStripGradientBegin => Header;
        public override Color StatusStripGradientEnd => Header;
        public override Color ToolStripDropDownBackground => Panel;
        public override Color ImageMarginGradientBegin => Panel;
        public override Color ImageMarginGradientMiddle => Panel;
        public override Color ImageMarginGradientEnd => Panel;
        public override Color MenuItemSelected => Selection;
        public override Color MenuItemBorder => Accent;
        public override Color MenuItemSelectedGradientBegin => Selection;
        public override Color MenuItemSelectedGradientEnd => Selection;
        public override Color MenuItemPressedGradientBegin => Field;
        public override Color MenuItemPressedGradientMiddle => Field;
        public override Color MenuItemPressedGradientEnd => Field;
        public override Color ButtonSelectedBorder => Accent;
        public override Color ButtonSelectedGradientBegin => Selection;
        public override Color ButtonSelectedGradientMiddle => Selection;
        public override Color ButtonSelectedGradientEnd => Selection;
        public override Color ButtonPressedGradientBegin => Selection;
        public override Color ButtonPressedGradientMiddle => Selection;
        public override Color ButtonPressedGradientEnd => Selection;
        public override Color ButtonCheckedGradientBegin => Selection;
        public override Color ButtonCheckedGradientMiddle => Selection;
        public override Color ButtonCheckedGradientEnd => Selection;
        public override Color ButtonCheckedHighlight => Selection;
        public override Color ButtonCheckedHighlightBorder => Accent;
        public override Color SeparatorDark => Border;
        public override Color SeparatorLight => Header;
        public override Color ToolStripBorder => Border;
    }
}
