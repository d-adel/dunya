using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;

namespace Dunya.Editor.Controls;

public class PanelHeader : TemplatedControl
{
    public static readonly StyledProperty<string?> TitleProperty =
        AvaloniaProperty.Register<PanelHeader, string?>(nameof(Title));

    public static readonly StyledProperty<string?> DetailProperty =
        AvaloniaProperty.Register<PanelHeader, string?>(nameof(Detail));

    public string? Title
    {
        get => GetValue(TitleProperty);
        set => SetValue(TitleProperty, value);
    }

    public string? Detail
    {
        get => GetValue(DetailProperty);
        set => SetValue(DetailProperty, value);
    }
}
