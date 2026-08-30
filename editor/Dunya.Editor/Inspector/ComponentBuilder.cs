using System;
using System.Collections.Generic;
using System.Text.Json;
using Avalonia.Controls;
using Dunya.Editor.Controls;

namespace Dunya.Editor.Inspector;

public sealed class ComponentBuilder
{
    private readonly JsonElement m_json;
    private readonly StackPanel m_rows = new();

    public ComponentBuilder(JsonElement json) => m_json = json;

    public bool IsEmpty => m_rows.Children.Count == 0;

    public ComponentBuilder Row(string label, Control content, string? unit = null)
    {
        m_rows.Children.Add(new FieldRow { Label = label, Content = content, Unit = unit });

        return this;
    }

    public ComponentBuilder Scalar(string label, string field, string? unit = null)
    {
        if (!m_json.TryGetProperty(field, out JsonElement value))
        {
            return this;
        }

        return Row(label, Fields.Lanes(new[] { value.GetDouble() }, null), unit);
    }

    public ComponentBuilder Vector(string label, string field, string? unit = null)
    {
        if (!m_json.TryGetProperty(field, out JsonElement value)
            || value.ValueKind != JsonValueKind.Array)
        {
            return this;
        }

        return Row(label, Fields.Lanes(Numbers(value), Fields.AxisLetters), unit);
    }

    public ComponentBuilder Euler(string label, string field)
    {
        if (!m_json.TryGetProperty(field, out JsonElement value)
            || value.ValueKind != JsonValueKind.Array)
        {
            return this;
        }

        IReadOnlyList<double> quaternion = Numbers(value);

        if (quaternion.Count != 4)
        {
            return Row(label, Fields.Lanes(quaternion, Fields.AxisLetters));
        }

        return Row(label, Fields.Lanes(ToEulerDegrees(quaternion), Fields.AxisLetters), "deg");
    }

    public ComponentBuilder Text(string label, string field)
    {
        if (!m_json.TryGetProperty(field, out JsonElement value))
        {
            return this;
        }

        return Row(label, Fields.Value(value.ToString()));
    }

    public ComponentBuilder Auto()
    {
        if (m_json.ValueKind != JsonValueKind.Object)
        {
            return this;
        }

        foreach (JsonProperty property in m_json.EnumerateObject())
        {
            Row(Humanised(property.Name), Widget(property.Value));
        }

        return this;
    }

    public Control Build() => m_rows;

    private static Control Widget(JsonElement value) => value.ValueKind switch
    {
        JsonValueKind.Number => Fields.Lanes(new[] { value.GetDouble() }, null),
        JsonValueKind.Array when AllNumbers(value) =>
            Fields.Lanes(Numbers(value), Fields.AxisLetters),
        JsonValueKind.True or JsonValueKind.False =>
            Fields.Value(value.GetBoolean() ? "true" : "false"),
        JsonValueKind.String => Fields.Value(value.GetString() ?? string.Empty),
        _ => Fields.Value(value.GetRawText())
    };

    private static bool AllNumbers(JsonElement array)
    {
        foreach (JsonElement item in array.EnumerateArray())
        {
            if (item.ValueKind != JsonValueKind.Number)
            {
                return false;
            }
        }

        return true;
    }

    private static IReadOnlyList<double> Numbers(JsonElement array)
    {
        var values = new List<double>();

        foreach (JsonElement item in array.EnumerateArray())
        {
            values.Add(item.ValueKind == JsonValueKind.Number ? item.GetDouble() : 0.0);
        }

        return values;
    }

    private static IReadOnlyList<double> ToEulerDegrees(IReadOnlyList<double> quaternion)
    {
        double w = quaternion[0];
        double x = quaternion[1];
        double y = quaternion[2];
        double z = quaternion[3];

        double sinPitch = 2.0 * (w * x - y * z);

        double pitch = Math.Abs(sinPitch) >= 1.0
            ? Math.CopySign(Math.PI / 2.0, sinPitch)
            : Math.Asin(sinPitch);

        double yaw = Math.Atan2(2.0 * (w * y + x * z), 1.0 - 2.0 * (x * x + y * y));
        double roll = Math.Atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (x * x + z * z));

        const double ToDegrees = 180.0 / Math.PI;

        return new[] { pitch * ToDegrees, yaw * ToDegrees, roll * ToDegrees };
    }

    private static string Humanised(string field)
    {
        if (field.Length == 0)
        {
            return field;
        }

        var text = new System.Text.StringBuilder();

        text.Append(char.ToUpperInvariant(field[0]));

        for (int index = 1; index < field.Length; index++)
        {
            if (char.IsUpper(field[index]) && !char.IsUpper(field[index - 1]))
            {
                text.Append(' ');
            }

            text.Append(field[index]);
        }

        return text.ToString();
    }
}
