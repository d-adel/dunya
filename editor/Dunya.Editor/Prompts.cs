using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Platform.Storage;

namespace Dunya.Editor;

public static class Prompts
{
    public static async Task<string?> Choose(
        Window owner,
        string title,
        string question,
        params string[] choices)
    {
        string? answer = null;

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 6
        };

        var dialog = new Window
        {
            Title = title,
            Width = 420,
            SizeToContent = SizeToContent.Height,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = false
        };

        foreach (string choice in choices)
        {
            var button = new Button { Content = choice, MinWidth = 96 };

            button.Click += (_, _) =>
            {
                answer = choice;
                dialog.Close();
            };

            buttons.Children.Add(button);
        }

        dialog.Content = new StackPanel
        {
            Margin = new Avalonia.Thickness(16),
            Spacing = 16,
            Children =
            {
                new TextBlock
                {
                    Text = question,
                    TextWrapping = Avalonia.Media.TextWrapping.Wrap
                },
                buttons
            }
        };

        await dialog.ShowDialog(owner);

        return answer;
    }

    public static async Task<string?> Text(Window owner, string title, string initial)
    {
        var box = new TextBox { Text = initial, Width = 320 };
        var ok = new Button { Content = "OK", IsDefault = true };
        var cancel = new Button { Content = "Cancel", IsCancel = true };

        string? answer = null;

        var dialog = new Window
        {
            Title = title,
            Width = 380,
            SizeToContent = SizeToContent.Height,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = false,
            Content = new StackPanel
            {
                Margin = new Avalonia.Thickness(12),
                Spacing = 10,
                Children =
                {
                    box,
                    new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        HorizontalAlignment = HorizontalAlignment.Right,
                        Spacing = 6,
                        Children = { ok, cancel }
                    }
                }
            }
        };

        ok.Click += (_, _) =>
        {
            answer = box.Text;
            dialog.Close();
        };

        cancel.Click += (_, _) => dialog.Close();

        await dialog.ShowDialog(owner);

        return string.IsNullOrWhiteSpace(answer) ? null : answer!.Trim();
    }

    public static async Task<string?> Folder(Window owner, string title)
    {
        IReadOnlyList<IStorageFolder> picked =
            await owner.StorageProvider.OpenFolderPickerAsync(
                new FolderPickerOpenOptions { Title = title, AllowMultiple = false }
            );

        return picked.Count == 0 ? null : picked[0].TryGetLocalPath();
    }

    public static async Task<string?> File(Window owner, string title, params string[] patterns)
    {
        var type = new FilePickerFileType("Assets") { Patterns = patterns };

        IReadOnlyList<IStorageFile> picked =
            await owner.StorageProvider.OpenFilePickerAsync(
                new FilePickerOpenOptions
                {
                    Title = title,
                    AllowMultiple = false,
                    FileTypeFilter = new[] { type }
                }
            );

        return picked.Count == 0 ? null : picked[0].TryGetLocalPath();
    }
}
