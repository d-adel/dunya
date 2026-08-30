using System;
using System.Text.Json;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Dunya.Editor.Controls;
using Dunya.Editor.Inspector;

namespace Dunya.Editor.Panels;

public partial class InspectorPanel : UserControl
{
    public InspectorPanel() => InitializeComponent();

    public Func<uint, string, string?>? Reader { get; set; }

    public void Show(WorldEntity? entity)
    {
        StackPanel cards = this.FindControl<StackPanel>("Cards")!;

        cards.Children.Clear();

        this.FindControl<TextBlock>("Subject")!.Text =
            entity is null ? "nothing selected" : $"entity {entity.Id}";

        if (entity is null)
        {
            return;
        }

        foreach (string component in entity.Components)
        {
            cards.Children.Add(Card(entity.Id, component));
        }
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
