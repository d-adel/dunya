using System;
using System.Collections.Generic;
using Avalonia.Logging;

namespace Dunya.Editor.Diagnostics;

public sealed class EditorLogSink : ILogSink
{
    private readonly LogEventLevel m_minimum;

    private readonly HashSet<string> m_seen = new(StringComparer.Ordinal);

    public EditorLogSink(LogEventLevel minimum) => m_minimum = minimum;

    public static Action<string>? Reported { get; set; }

    public bool IsEnabled(LogEventLevel level, string area) => level >= m_minimum;

    public void Log(LogEventLevel level, string area, object? source, string messageTemplate)
    {
        Report(level, area, messageTemplate);
    }

    public void Log(
        LogEventLevel level,
        string area,
        object? source,
        string messageTemplate,
        params object?[] propertyValues)
    {
        Report(level, area, Fill(messageTemplate, propertyValues));
    }

    private void Report(LogEventLevel level, string area, string message)
    {
        if (!IsEnabled(level, area))
        {
            return;
        }

        string line = $"[avalonia {level} {area}] {message}";

        lock (m_seen)
        {
            if (!m_seen.Add(line))
            {
                return;
            }
        }

        Reported?.Invoke(line);
    }

    private static string Fill(string template, object?[] values)
    {
        string filled = template;

        foreach (object? value in values)
        {
            int open = filled.IndexOf('{');

            if (open < 0)
            {
                break;
            }

            int close = filled.IndexOf('}', open);

            if (close < 0)
            {
                break;
            }

            filled = filled.Remove(open, close - open + 1)
                .Insert(open, value?.ToString() ?? "null");
        }

        return filled;
    }
}
