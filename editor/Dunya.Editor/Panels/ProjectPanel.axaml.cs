using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Controls.Models.TreeDataGrid;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public sealed class ProjectItem
{
    public ProjectItem(string name, string detail, string kind, string? path)
    {
        Name = name;
        Detail = detail;
        Kind = kind;
        Path = path;
    }

    public string Name { get; }

    public string Detail { get; }

    public string Kind { get; }

    public string? Path { get; }

    public ObservableCollection<ProjectItem> Children { get; } = new();
}

public partial class ProjectPanel : UserControl
{
    private const string WorldSuffix = ".world.json";

    private readonly ObservableCollection<ProjectItem> m_roots = new();

    private readonly HierarchicalTreeDataGridSource<ProjectItem> m_source;

    public ProjectPanel()
    {
        AvaloniaXamlLoader.Load(this);

        m_source = new HierarchicalTreeDataGridSource<ProjectItem>(m_roots)
        {
            Columns =
            {
                new HierarchicalExpanderColumn<ProjectItem>(
                    new TextColumn<ProjectItem, string>("Name", item => item.Name),
                    item => item.Children
                ),
                new TextColumn<ProjectItem, string>("Kind", item => item.Kind),
                new TextColumn<ProjectItem, string>("Detail", item => item.Detail)
            }
        };

        TreeDataGrid grid = this.FindControl<TreeDataGrid>("Contents")!;

        grid.Source = m_source;

        grid.DoubleTapped += (_, _) =>
        {
            if (m_source.RowSelection?.SelectedItem is not ProjectItem item)
            {
                return;
            }

            if (item.Kind == "world" && item.Path != null)
            {
                OpenWorld?.Invoke(
                    System.IO.Path.GetFileName(item.Path)
                        .Replace(WorldSuffix, string.Empty)
                );
            }
        };

        this.FindControl<Button>("RefreshButton")!.Click += (_, _) => Refresh?.Invoke();
        this.FindControl<Button>("NewWorldButton")!.Click += (_, _) => NewWorld?.Invoke();
        this.FindControl<Button>("ImportButton")!.Click += (_, _) => Import?.Invoke();
    }

    public event Action<string>? OpenWorld;

    public event Action? Refresh;

    public event Action? NewWorld;

    public event Action? Import;

    public void Show(string projectRoot, string currentWorld)
    {
        this.FindControl<TextBlock>("Root")!.Text =
            System.IO.Path.GetFullPath(projectRoot);

        m_roots.Clear();
        m_source.Items = m_roots;

        if (!Directory.Exists(projectRoot))
        {
            m_roots.Add(new ProjectItem(projectRoot, "missing", "folder", null));

            return;
        }

        foreach (ProjectItem item in Walk(projectRoot, currentWorld))
        {
            m_roots.Add(item);
        }
    }

    private static ProjectItem[] Walk(string folder, string currentWorld)
    {
        DirectoryInfo directory = new(folder);

        ProjectItem[] folders = directory
            .EnumerateDirectories()
            .OrderBy(entry => entry.Name, StringComparer.OrdinalIgnoreCase)
            .Select(entry =>
            {
                ProjectItem item = new(entry.Name, string.Empty, "folder", entry.FullName);

                foreach (ProjectItem child in Walk(entry.FullName, currentWorld))
                {
                    item.Children.Add(child);
                }

                return item;
            })
            .ToArray();

        ProjectItem[] files = directory
            .EnumerateFiles()
            .OrderBy(entry => entry.Name, StringComparer.OrdinalIgnoreCase)
            .Select(entry => Describe(entry, currentWorld))
            .ToArray();

        return folders.Concat(files).ToArray();
    }

    private static ProjectItem Describe(FileInfo file, string currentWorld)
    {
        bool world = file.Name.EndsWith(WorldSuffix, StringComparison.OrdinalIgnoreCase);

        string name = world
            ? file.Name[..^WorldSuffix.Length]
            : file.Name;

        string kind = world
            ? "world"
            : file.Extension.TrimStart('.').ToLowerInvariant();

        string detail = world && name == currentWorld
            ? "open"
            : Size(file.Length);

        return new ProjectItem(name, detail, kind, file.FullName);
    }

    private static string Size(long bytes)
    {
        if (bytes < 1024)
        {
            return bytes + " B";
        }

        if (bytes < 1024 * 1024)
        {
            return (bytes / 1024) + " KB";
        }

        return (bytes / (1024 * 1024)) + " MB";
    }
}
