using System;
using System.IO;
using Avalonia;
using Avalonia.Logging;
using Dunya.Editor.Diagnostics;

namespace Dunya.Editor;

internal static class Program
{
    public static string ProjectRoot { get; private set; } = "projects/demo";

    public static bool ProjectGiven { get; private set; }

    public static string World { get; private set; } = "main";

    public static bool AutoPlay { get; private set; }

    public static string? AuthorPath { get; private set; }


    [System.STAThread]
    public static void Main(string[] args)
    {
        for (int index = 0; index < args.Length; ++index)
        {
            if (args[index] == "--new-project" && index + 2 < args.Length)
            {
                int code =
                    DunyaNative.dunya_project_create(args[index + 1], args[index + 2]);

                Console.WriteLine(
                    code == 0
                        ? "project created at " + args[index + 1]
                        : "project NOT created: " + DunyaNative.LastError()
                );

                return;
            }

            if (args[index] == "--project" && index + 1 < args.Length)
            {
                ProjectRoot = args[index + 1];
            }


            if (args[index] == "--play")
            {
                AutoPlay = true;
            }

            if (args[index] == "--world" && index + 1 < args.Length)
            {
                World = args[index + 1];
            }

            if (args[index] == "--author" && index + 1 < args.Length)
            {
                AuthorPath = args[index + 1];
            }
        }

        if (!ProjectGiven)
        {
            EditorSettings remembered = EditorSettings.Load();

            if (remembered.ProjectRoot != null
                && Directory.Exists(remembered.ProjectRoot))
            {
                ProjectRoot = remembered.ProjectRoot;

                if (remembered.World != null)
                {
                    World = remembered.World;
                }
            }
        }

        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp()
    {
        Logger.Sink = new EditorLogSink(LogEventLevel.Warning);

        return AppBuilder.Configure<App>().UsePlatformDetect();
    }
}
