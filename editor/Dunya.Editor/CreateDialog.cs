using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;

namespace Dunya.Editor;

public sealed record CreatableKind(string Name, string Detail);

public static class CreateDialog
{
    private static readonly CreatableKind[] Kinds =
    {
        new("Box", "A deformable box of signed distance field"),
        new("Sphere", "A deformable sphere of signed distance field"),
        new("Cylinder", "A deformable cylinder of signed distance field"),
        new("Camera", "A Lens the runtime renders through"),
        new("Light", "A DirectionalLight the world is lit by"),
        new("Environment", "The sky and ambient the world is drawn under")
    };

    public static async Task<string?> Ask(Window owner)
    {
        var search = new TextBox { PlaceholderText = "Search", Width = 380 };

        var shown = new ObservableCollection<CreatableKind>(Kinds);

        var list = new ListBox
        {
            ItemsSource = shown,
            Height = 260,
            SelectedIndex = 0
        };

        var detail = new TextBlock
        {
            TextWrapping = Avalonia.Media.TextWrapping.Wrap,
            MinHeight = 34,
            Opacity = 0.7
        };

        var create = new Button { Content = "Create", IsDefault = true };
        var cancel = new Button { Content = "Cancel", IsCancel = true };

        string? chosen = null;

        void Refill()
        {
            string filter = search.Text ?? string.Empty;

            shown.Clear();

            foreach (CreatableKind kind in Kinds.Where(entry =>
                         filter.Length == 0
                         || entry.Name.Contains(
                             filter,
                             StringComparison.OrdinalIgnoreCase
                         )))
            {
                shown.Add(kind);
            }

            if (shown.Count > 0)
            {
                list.SelectedIndex = 0;
            }
        }

        void Describe()
        {
            detail.Text = list.SelectedItem is CreatableKind kind
                ? kind.Detail
                : string.Empty;
        }

        search.TextChanged += (_, _) => Refill();
        list.SelectionChanged += (_, _) => Describe();

        Describe();

        var dialog = new Window
        {
            Title = "Create Entity",
            Width = 420,
            SizeToContent = SizeToContent.Height,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = false,
            Content = new StackPanel
            {
                Margin = new Avalonia.Thickness(12),
                Spacing = 10,
                Children =
                {
                    search,
                    list,
                    detail,
                    new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        HorizontalAlignment = HorizontalAlignment.Right,
                        Spacing = 6,
                        Children = { create, cancel }
                    }
                }
            }
        };

        void Accept()
        {
            if (list.SelectedItem is CreatableKind kind)
            {
                chosen = kind.Name;
            }

            dialog.Close();
        }

        create.Click += (_, _) => Accept();
        cancel.Click += (_, _) => dialog.Close();
        list.DoubleTapped += (_, _) => Accept();

        list.KeyDown += (_, key) =>
        {
            if (key.Key == Key.Enter)
            {
                Accept();
            }
        };

        await dialog.ShowDialog(owner);

        return chosen;
    }

    public static IReadOnlyList<string> Names =>
        Kinds.Select(kind => kind.Name).ToArray();
}
