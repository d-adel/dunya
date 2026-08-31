using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Controls.Models.TreeDataGrid;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public partial class EntitiesPanel : UserControl
{
    private readonly ObservableCollection<WorldArchetype> m_rows = new();
    private readonly FlatTreeDataGridSource<WorldArchetype> m_source;

    private IReadOnlyList<WorldArchetype> m_all = Array.Empty<WorldArchetype>();

    private uint? m_focus;

    public EntitiesPanel()
    {
        InitializeComponent();

        m_source = new FlatTreeDataGridSource<WorldArchetype>(m_rows)
        {
            Columns =
            {
                new TextColumn<WorldArchetype, int>(
                    "Count", row => row.Count, GridLength.Auto),
                new TextColumn<WorldArchetype, string>(
                    "Archetype",
                    row => row.Alias,
                    new GridLength(1, GridUnitType.Star))
            }
        };

        TreeDataGrid grid = this.FindControl<TreeDataGrid>("Entities")!;

        grid.Source = m_source;

        m_source.RowSelection!.SelectionChanged += (_, _) =>
        {
            uint? focus = m_focus;

            m_focus = null;

            Picked?.Invoke(m_source.RowSelection.SelectedItem, focus);
        };

        this.FindControl<TextBox>("Filter")!.TextChanged += (_, _) => Refill();

        this.FindControl<Button>("Add")!.Click += (_, _) => AddRequested?.Invoke();
    }

    public event Action<WorldArchetype?, uint?>? Picked;

    public event Action? AddRequested;

    public void Show(IReadOnlyList<WorldEntity> contents)
    {
        m_all = WorldArchetype.Group(contents);

        Refill();

        this.FindControl<TextBlock>("Count")!.Text =
            $"{m_all.Count} archetypes, {contents.Count} entities";
    }

    public void Select(uint? id)
    {
        if (id == null)
        {
            m_source.RowSelection!.Clear();

            return;
        }

        WorldArchetype? owner =
            m_rows.FirstOrDefault(archetype => archetype.Contains(id.Value));

        if (owner == null)
        {
            return;
        }

        m_focus = id;

        m_source.RowSelection!.SelectedIndex = new(m_rows.IndexOf(owner));
    }

    private void Refill()
    {
        string filter = this.FindControl<TextBox>("Filter")!.Text ?? string.Empty;

        m_rows.Clear();

        foreach (WorldArchetype archetype in m_all)
        {
            if (
                filter.Length == 0
                || (archetype.Alias + " " + archetype.Detail).Contains(
                    filter,
                    StringComparison.OrdinalIgnoreCase
                )
            )
            {
                m_rows.Add(archetype);
            }
        }
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
