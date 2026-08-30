using Avalonia;
using Avalonia.Logging;
using Dunya.Editor.Diagnostics;

namespace Dunya.Editor;

internal static class Program
{
    [System.STAThread]
    public static void Main(string[] args) =>
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

    public static AppBuilder BuildAvaloniaApp()
    {
        Logger.Sink = new EditorLogSink(LogEventLevel.Warning);

        return AppBuilder.Configure<App>().UsePlatformDetect();
    }
}
