using System;
using System.Text.Json;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Dunya.Editor.Controls;
using Dunya.Editor.Inspector;

namespace Dunya.Editor.Panels;

public partial class InspectorPanel : UserControl
{
    private WorldArchetype? m_archetype;

    private int m_at;

    public InspectorPanel()
    {
        InitializeComponent();

        this.FindControl<Button>("Previous")!.Click += (_, _) => Step(-1);
        this.FindControl<Button>("Next")!.Click += (_, _) => Step(1);
    }

    public Func<uint, string, string?>? Reader { get; set; }

    public event Action<uint?>? Focused;

    public void Show(WorldArchetype? archetype, uint? focus)
    {
        m_archetype = archetype;

        m_at = archetype is null || focus is null
            ? 0
            : Math.Max(0, Array.IndexOf(archetype.Entities, focus.Value));

        Refresh();
    }

    private void Step(int by)
    {
        if (m_archetype is null || m_archetype.Count == 0)
        {
            return;
        }

        m_at = (m_at + by + m_archetype.Count) % m_archetype.Count;

        Refresh();
    }

    private void Refresh()
    {
        StackPanel cards = this.FindControl<StackPanel>("Cards")!;

        cards.Children.Clear();

        StackPanel stepper = this.FindControl<StackPanel>("Stepper")!;

        if (m_archetype is null || m_archetype.Count == 0)
        {
            this.FindControl<TextBlock>("Subject")!.Text = "nothing selected";
            stepper.IsVisible = false;

            Focused?.Invoke(null);

            return;
        }

        uint entity = m_archetype.Entities[m_at];

        this.FindControl<TextBlock>("Subject")!.Text = m_archetype.Alias;

        stepper.IsVisible = true;

        this.FindControl<TextBlock>("Which")!.Text =
            $"entity {entity}   {m_at + 1} of {m_archetype.Count}";

        foreach (string component in m_archetype.Components)
        {
            cards.Children.Add(Card(entity, component));
        }

        Focused?.Invoke(entity);
    }

    private ComponentCard Card(uint entity, string component)
    {
        var card = new ComponentCard { Header = component };

        string? json = Reader?.Invoke(entity, component);

        if (json is null)
        {
            card.IsTag = true;
            card.Note = "runtime";

            return card;
        }

        using JsonDocument document = JsonDocument.Parse(json);

        var builder = new ComponentBuilder(document.RootElement.Clone());

        Avalonia.Controls.Control body = DrawerRegistry.For(component).Draw(builder);

        if (builder.IsEmpty)
        {
            card.IsTag = true;
            card.Note = "tag";

            return card;
        }

        card.Content = body;

        return card;
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
