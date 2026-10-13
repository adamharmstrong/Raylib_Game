using System.Reflection;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        var assembly = args.Length > 0
            ? new System.Runtime.Loader.AssemblyLoadContext("Editor build verification").LoadFromAssemblyPath(Path.GetFullPath(args[0]))
            : Assembly.Load("LevelEditor");
        if (args.Length > 0 && !assembly.Location.Equals(Path.GetFullPath(args[0]), StringComparison.OrdinalIgnoreCase))
            throw new Exception("The requested editor build was not loaded.");
        var type = assembly.GetType("PowerPulleyPanic.LevelEditor.EditorForm")!;
        using var form = (Form)Activator.CreateInstance(type, BindingFlags.Instance | BindingFlags.NonPublic,
            null, new object?[] { null }, null)!;
        Exception? passageLayoutError = null;
        using (var timer = new System.Windows.Forms.Timer { Interval = 100 })
        {
            timer.Tick += (_, _) =>
            {
                var dialog = Application.OpenForms.Cast<Form>().FirstOrDefault(item => item.Text == "Tile Passage");
                if (dialog == null) return;
                timer.Stop();
                void CheckBounds(Control parent)
                {
                    foreach (Control child in parent.Controls)
                    {
                        if (!parent.ClientRectangle.Contains(child.Bounds))
                            passageLayoutError = new Exception($"Tile Passage clips '{child.Text}': {child.Bounds} outside {parent.ClientRectangle}.");
                        CheckBounds(child);
                    }
                }
                CheckBounds(dialog);
                Console.WriteLine($"Tile Passage controls fit inside {dialog.ClientSize.Width} x {dialog.ClientSize.Height}.");
                dialog.DialogResult = DialogResult.Cancel;
                dialog.Close();
            };
            timer.Start();
            type.GetMethod("EditTilePassage", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(form, null);
        }
        if (passageLayoutError != null) throw passageLayoutError;
        var selector = (ComboBox)type.GetField("tileSheetSelector", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(form)!;
        var palette = type.GetField("tilePalette", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(form)!;
        var size = (Size)palette.GetType().GetProperty("SheetSize", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(palette)!;
        if (selector.SelectedIndex != 0 || size.Width <= 0 || size.Height <= 0)
            throw new Exception("The default sprite sheet must load at startup without changing the dropdown.");
        var row = (RowStyle)type.GetField("spritePreviewRow", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(form)!;
        if (row.Height <= 0) throw new Exception("The initial sprite sheet preview must have a visible height.");
        Console.WriteLine($"Startup sheet loaded without dropdown interaction: {size.Width} x {size.Height}.");
        _ = form.Handle;
        object Field(string name) => type.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(form)!;
        void Invoke(string name, params object[] args) => type.GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(form, args);
        var assetNames = (string[])type.GetField("AssetNames", BindingFlags.Static | BindingFlags.NonPublic)!.GetValue(null)!;
        var assetCommandIndex = type.GetMethod("GetAssetCommandIndex", BindingFlags.Static | BindingFlags.NonPublic)!;
        var createAssetDragData = type.GetMethod("CreateAssetDragData", BindingFlags.Static | BindingFlags.NonPublic)!;
        for (var index = 0; index < assetNames.Length; ++index)
        {
            var expectedCommand = index < 34 ? index : index + 1;
            var actualCommand = (int)assetCommandIndex.Invoke(null, new object[] { index })!;
            if (actualCommand != expectedCommand)
                throw new Exception($"Content asset '{assetNames[index]}' maps to command {actualCommand}, expected {expectedCommand}.");
            var dragData = (DataObject)createAssetDragData.Invoke(null, new object[] { index })!;
            dragData.TryGetData<string>(DataFormats.UnicodeText, out var draggedCommand);
            if (draggedCommand != expectedCommand.ToString(System.Globalization.CultureInfo.InvariantCulture))
                throw new Exception($"Dragging '{assetNames[index]}' sends command {draggedCommand}, expected {expectedCommand}.");
        }
        using var host = new Form { ShowInTaskbar = false, Opacity = 0, ClientSize = new Size(1100, 900) };
        foreach (Control child in form.Controls.Cast<Control>().ToArray()) host.Controls.Add(child);
        host.Show();
        Invoke("ApplyDefaultLayout");
        var root = (SplitContainer)Field("rootSplit");
        root.Panel1MinSize = 0;
        root.SplitterDistance = 320;
        Application.DoEvents();
        var document = Field("document");
        foreach (var key in new[] { "user1", "user2" })
            document.GetType().GetMethod("AddTileLayer", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(document, new object[] { key });
        Invoke("RefreshTileLayers");
        Application.DoEvents();
        var list = (ListBox)Field("visibleLayers");
        list.SelectedIndex = 0;
        if (((ToolStripButton)Field("moveLayerUp")).Enabled || ((ToolStripButton)Field("moveLayerDown")).Enabled)
            throw new Exception("Default layers must not expose reorder actions.");
        list.SelectedIndex = 5;
        if (!((ToolStripButton)Field("moveLayerUp")).Enabled || !((ToolStripButton)Field("moveLayerDown")).Enabled)
            throw new Exception("Custom layer reorder actions must respect the fixed default boundary.");
        var listType = list.GetType();
        var overlayMaskBefore = (int)Field("collisionLayerMask");
        var lockedMaskBefore = (int)Field("lockedLayerMask");
        var visibleBefore = (bool)listType.GetMethod("GetItemChecked", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list, new object[] { 0 })!;
        var glyphWidth = (int)listType.GetProperty("GlyphWidth", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(list)!;
        var firstRow = list.GetItemRectangle(0);
        void ClickCollision() => listType.GetMethod("OnMouseDown", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list,
            new object[] { new MouseEventArgs(MouseButtons.Left, 1, firstRow.Right - glyphWidth * 2 + glyphWidth / 2,
                firstRow.Top + firstRow.Height / 2, 0) });
        ClickCollision();
        if ((int)Field("collisionLayerMask") == overlayMaskBefore || (int)Field("lockedLayerMask") != lockedMaskBefore ||
            (bool)listType.GetMethod("GetItemChecked", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list, new object[] { 0 })! != visibleBefore)
            throw new Exception("Collision toggle must change only the overlay, without locking or hiding the layer.");
        ClickCollision();
        if ((int)Field("collisionLayerMask") != overlayMaskBefore) throw new Exception("Collision toggle must restore the layer's overlay.");
        Console.WriteLine("Per-layer collision overlay toggle checks passed.");
        listType.GetMethod("BeginRename", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list, new object[] { 0 });
        if (list.Controls.OfType<TextBox>().Any()) throw new Exception("Double-click editing must reject default layer names.");
        var nameBounds = (Rectangle)listType.GetMethod("NameBounds", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list, new object[] { 5 })!;
        listType.GetMethod("OnMouseDoubleClick", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list,
            new object[] { new MouseEventArgs(MouseButtons.Left, 2, nameBounds.Left + 5, nameBounds.Top + nameBounds.Height / 2, 0) });
        var editor = list.Controls.OfType<TextBox>().Single();
        var typingKey = new KeyEventArgs(Keys.W);
        Invoke("OnEditorKeyDown", form, typingKey);
        if (typingKey.Handled || typingKey.SuppressKeyPress) throw new Exception("Editing a layer name must bypass viewport keyboard shortcuts.");
        editor.Text = "Machinery Front";
        typeof(TextBox).GetMethod("OnKeyDown", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(editor, new object[] { new KeyEventArgs(Keys.Enter) });
        if (list.Items[5].ToString() != "Machinery Front" || list.Controls.OfType<TextBox>().Any())
            throw new Exception("Inline rename must update the displayed layer and close its text field.");
        listType.GetMethod("BeginRename", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(list, new object[] { 5 });
        editor = list.Controls.OfType<TextBox>().Single();
        editor.Text = "Cancelled name";
        typeof(TextBox).GetMethod("OnKeyDown", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(editor, new object[] { new KeyEventArgs(Keys.Escape) });
        if (list.Items[5].ToString() != "Machinery Front" || list.Controls.OfType<TextBox>().Any())
            throw new Exception("Escape must cancel inline renaming.");
        Invoke("ToggleLayerLock", 4);
        Invoke("ToggleLayerLock", 2);
        var mask = (int)Field("lockedLayerMask");
        if ((mask & (1 << 4)) == 0 || (mask & (1 << 2)) == 0) throw new Exception("Each layer must lock independently.");
        Invoke("ToggleLayerLock", 4);
        if (((int)Field("lockedLayerMask") & (1 << 4)) != 0 || ((int)Field("lockedLayerMask") & (1 << 2)) == 0)
            throw new Exception("Unlocking a user layer must preserve the other layer's lock.");
        document.GetType().GetMethod("MoveTileLayer", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(document, new object[] { "user1", 1 });
        Invoke("RefreshTileLayers");
        if (list.Items[6].ToString() != "Machinery Front" || list.SelectedIndex != 6)
            throw new Exception("Reordering must move the renamed layer and retain its selection.");
        if (list.Items[1].ToString() != "Middleground" || list.Items[2].ToString() != "Background")
            throw new Exception("Middleground must sit between Foreground and Background.");
        document.GetType().GetMethod("MoveTileLayer", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(document, new object[] { "user2", -1 });
        Invoke("RefreshTileLayers");
        if (list.Items[0].ToString() != "User Layer 2" || list.Items[1].ToString() != "Foreground" || list.Items[2].ToString() != "Middleground")
            throw new Exception("The top custom layer must jump above the fixed default block.");
        list.SelectedIndex = 0;
        if (((ToolStripButton)Field("moveLayerUp")).Enabled || !((ToolStripButton)Field("moveLayerDown")).Enabled)
            throw new Exception("A custom layer above the defaults must be able to jump back down.");
        Console.WriteLine("Layer UI checks passed: fixed defaults, reorder boundaries, inline rename, per-layer locks, and selection preservation.");

        using var screenshot = new Bitmap(root.Panel1.Width, root.Panel1.Height);
        root.Panel1.DrawToBitmap(screenshot, new Rectangle(Point.Empty, screenshot.Size));
        screenshot.Save(Path.Combine(AppContext.BaseDirectory, "layer-ui-preview.png"));
    }
}
