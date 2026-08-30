using System;
using System.Collections.Generic;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public partial class EntitiesPanel : UserControl
{
    public EntitiesPanel()
    {
        InitializeComponent();

        ListBox list = this.FindControl<ListBox>("Entities")!;

        list.SelectionChanged +=
            (_, _) => Picked?.Invoke(list.SelectedItem as WorldEntity);
    }

    public event Action<WorldEntity?>? Picked;

    public void Show(IReadOnlyList<WorldEntity> contents)
    {
        this.FindControl<ListBox>("Entities")!.ItemsSource = contents;

        this.FindControl<TextBlock>("Count")!.Text = $"{contents.Count} entities";
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
