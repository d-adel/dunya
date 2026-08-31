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
    private readonly ObservableCollection<WorldEntity> m_rows = new();
    private readonly FlatTreeDataGridSource<WorldEntity> m_source;

    private IReadOnlyList<WorldEntity> m_all = Array.Empty<WorldEntity>();

    public EntitiesPanel()
    {
        InitializeComponent();

        m_source = new FlatTreeDataGridSource<WorldEntity>(m_rows)
        {
            Columns =
            {
                new TextColumn<WorldEntity, uint>("Id", row => row.Id),
                new TextColumn<WorldEntity, string>("Kind", row => row.Kind),
                new TextColumn<WorldEntity, string>("Components", row => row.Summary)
            }
        };

        TreeDataGrid grid = this.FindControl<TreeDataGrid>("Entities")!;

        grid.Source = m_source;

        m_source.RowSelection!.SelectionChanged +=
            (_, _) => Picked?.Invoke(m_source.RowSelection.SelectedItem);

        this.FindControl<TextBox>("Filter")!.TextChanged += (_, _) => Refill();
    }

    public event Action<WorldEntity?>? Picked;

    public void Show(IReadOnlyList<WorldEntity> contents)
    {
        m_all = contents;

        Refill();

        this.FindControl<TextBlock>("Count")!.Text = $"{contents.Count} entities";
    }

    public void Select(uint? id)
    {
        if (id == null)
        {
            m_source.RowSelection!.Clear();

            return;
        }

        for (int index = 0; index < m_rows.Count; ++index)
        {
            if (m_rows[index].Id == id.Value)
            {
                m_source.RowSelection!.SelectedIndex = new Avalonia.Controls.IndexPath(index);

                return;
            }
        }
    }

    private void Refill()
    {
        string filter = this.FindControl<TextBox>("Filter")?.Text ?? string.Empty;

        m_rows.Clear();

        foreach (WorldEntity entity in m_all)
        {
            if (filter.Length > 0
                && !entity.Kind.Contains(filter, StringComparison.OrdinalIgnoreCase)
                && !entity.Summary.Contains(filter, StringComparison.OrdinalIgnoreCase)
                && !entity.Id.ToString().Contains(filter, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            m_rows.Add(entity);
        }
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
