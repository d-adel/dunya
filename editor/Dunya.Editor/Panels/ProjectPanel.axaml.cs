using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using Avalonia.Controls;
using Avalonia.Controls.Models.TreeDataGrid;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public sealed class ProjectItem
{
    public ProjectItem(string name, string detail, string kind, string? payload)
    {
        Name = name;
        Detail = detail;
        Kind = kind;
        Payload = payload;
    }

    public string Name { get; }

    public string Detail { get; }

    public string Kind { get; }

    public string? Payload { get; }

    public ObservableCollection<ProjectItem> Children { get; } = new();
}

public partial class ProjectPanel : UserControl
{
    private readonly ObservableCollection<ProjectItem> m_roots = new();

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
            if (m_source.RowSelection?.SelectedItem is ProjectItem item
                && item.Kind == "world"
                && item.Payload != null)
            {
                OpenWorld?.Invoke(item.Payload);
            }
        };

        this.FindControl<Button>("RefreshButton")!.Click += (_, _) => Refresh?.Invoke();
        this.FindControl<Button>("NewWorldButton")!.Click += (_, _) => NewWorld?.Invoke();
        this.FindControl<Button>("ImportButton")!.Click += (_, _) => Import?.Invoke();
    }

    private readonly HierarchicalTreeDataGridSource<ProjectItem> m_source;
    public event Action<string>? OpenWorld;

    public event Action? Refresh;

    public event Action? NewWorld;

    public event Action? Import;

    public void Show(
        string projectRoot,
        string currentWorld,
        IReadOnlyList<string> worlds,
        IReadOnlyList<string> assetLines
    )
    {
        this.FindControl<TextBlock>("Root")!.Text = projectRoot;

        m_roots.Clear();
        m_source.Items = m_roots;

        var worldGroup = new ProjectItem("Worlds", worlds.Count + " open by double-click", "group", null);

        foreach (string world in worlds)
        {
            worldGroup.Children.Add(
                new ProjectItem(world, world == currentWorld ? "open" : string.Empty, "world", world)
            );
        }

        m_roots.Add(worldGroup);

        var byType = new Dictionary<string, ProjectItem>();

        foreach (string line in assetLines)
        {
            string[] parts = line.Split('\t');

            if (parts.Length != 3)
            {
                continue;
            }

            if (!byType.TryGetValue(parts[1], out ProjectItem? group))
            {
                group = new ProjectItem(parts[1], string.Empty, "group", null);
                byType[parts[1]] = group;
                m_roots.Add(group);
            }

            group.Children.Add(
                new ProjectItem(Path.GetFileName(parts[2]), parts[0], "asset", parts[2])
            );
        }
    }
}
