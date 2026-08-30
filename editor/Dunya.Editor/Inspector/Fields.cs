using System;
using System.Collections.Generic;
using System.Globalization;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Markup.Xaml.MarkupExtensions;

namespace Dunya.Editor.Inspector;

internal static class Fields
{
    private static readonly string[] AxisBrushKeys =
    {
        "EditorAxisX", "EditorAxisY", "EditorAxisZ", "EditorAxisW"
    };

    public static readonly string[] AxisLetters = { "X", "Y", "Z", "W" };

    public static string Format(double value) =>
        value.ToString("0.####", CultureInfo.InvariantCulture);

    public static Control Lanes(IReadOnlyList<double> values, IReadOnlyList<string>? letters)
    {
        var grid = new Grid { ColumnSpacing = 4 };

        for (int lane = 0; lane < values.Count; lane++)
        {
            grid.ColumnDefinitions.Add(new ColumnDefinition(1, GridUnitType.Star));
        }

        for (int lane = 0; lane < values.Count; lane++)
        {
            var content = new DockPanel();

            if (letters is not null && lane < letters.Count)
            {
                var axis = new TextBlock { Text = letters[lane] };

                axis.Classes.Add("axisLabel");
                axis.Bind(
                    TextBlock.ForegroundProperty,
                    new DynamicResourceExtension(
                        AxisBrushKeys[Math.Min(lane, AxisBrushKeys.Length - 1)]));

                DockPanel.SetDock(axis, Avalonia.Controls.Dock.Left);
                content.Children.Add(axis);
            }

            var text = new TextBlock
            {
                Text = Format(values[lane]),
                HorizontalAlignment = HorizontalAlignment.Right
            };

            text.Classes.Add("fieldValue");
            content.Children.Add(text);

            var border = new Border { Child = content };

            border.Classes.Add("fieldLane");

            Grid.SetColumn(border, lane);
            grid.Children.Add(border);
        }

        return grid;
    }

    public static Control Value(string text)
    {
        var block = new TextBlock { Text = text };

        block.Classes.Add("fieldValue");

        var border = new Border { Child = block };

        border.Classes.Add("fieldLane");

        return border;
    }
}
