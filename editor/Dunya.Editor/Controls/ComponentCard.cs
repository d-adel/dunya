using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;

namespace Dunya.Editor.Controls;

public class ComponentCard : HeaderedContentControl
{
    public static readonly StyledProperty<bool> IsExpandedProperty =
        AvaloniaProperty.Register<ComponentCard, bool>(nameof(IsExpanded), true);

    public static readonly StyledProperty<bool> IsTagProperty =
        AvaloniaProperty.Register<ComponentCard, bool>(nameof(IsTag));

    public static readonly StyledProperty<string?> NoteProperty =
        AvaloniaProperty.Register<ComponentCard, string?>(nameof(Note));

    public bool IsExpanded
    {
        get => GetValue(IsExpandedProperty);
        set => SetValue(IsExpandedProperty, value);
    }

    public bool IsTag
    {
        get => GetValue(IsTagProperty);
        set => SetValue(IsTagProperty, value);
    }

    public string? Note
    {
        get => GetValue(NoteProperty);
        set => SetValue(NoteProperty, value);
    }
}
