using Avalonia;
using Avalonia.Controls;

namespace Dunya.Editor.Controls;

public class FieldRow : ContentControl
{
    public static readonly StyledProperty<string?> LabelProperty =
        AvaloniaProperty.Register<FieldRow, string?>(nameof(Label));

    public static readonly StyledProperty<string?> UnitProperty =
        AvaloniaProperty.Register<FieldRow, string?>(nameof(Unit));

    public string? Label
    {
        get => GetValue(LabelProperty);
        set => SetValue(LabelProperty, value);
    }

    public string? Unit
    {
        get => GetValue(UnitProperty);
        set => SetValue(UnitProperty, value);
    }
}
