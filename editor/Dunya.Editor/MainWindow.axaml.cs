using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Dock.Avalonia.Controls;
using Dock.Model.Avalonia;
using Dunya.Editor.Diagnostics;
using Dunya.Editor.Panels;
using Dunya.Editor.Shell;

namespace Dunya.Editor;

public partial class MainWindow : Window
{
    private readonly string m_logPath =
        Path.Combine(AppContext.BaseDirectory, "spike.log");

    private readonly ViewportHost m_viewport = new();
    private readonly EntitiesPanel m_entities = new();
    private readonly InspectorPanel m_inspector = new();
    private readonly ConsolePanel m_console = new();

    private readonly Factory m_factory = new();

    private EditorLayout m_layout = null!;

    public MainWindow()
    {
        InitializeComponent();

        File.WriteAllText(m_logPath, string.Empty);

        EditorLogSink.Reported = Append;

        m_viewport.Reported += Append;
        m_viewport.WorldOpened += () => Dispatcher.UIThread.Post(ShowContents);

        m_entities.Picked += m_inspector.Show;
        m_inspector.Reader = m_viewport.Component;

        BuildLayout();

        this.FindControl<MenuItem>("ResetLayoutItem")!.Click += (_, _) => BuildLayout();

        Closing += (_, _) => m_viewport.Shutdown();

        Opened += (_, _) =>
        {
            Append($"window opened   top level hwnd=0x{TryGetPlatformHandle()?.Handle ?? IntPtr.Zero:X}");

            int seconds = AutoCloseSeconds();

            if (seconds <= 0)
            {
                return;
            }

            DispatcherTimer.RunOnce(
                () =>
                {
                    Append("spike over");
                    Close();
                },
                TimeSpan.FromSeconds(seconds)
            );
        };
    }

    private void BuildLayout()
    {
        m_layout = EditorLayout.Build(
            m_factory,
            m_entities,
            m_viewport,
            m_inspector,
            m_console
        );

        DockControl dock = this.FindControl<DockControl>("Dock")!;

        dock.Factory = m_factory;
        dock.InitializeFactory = true;
        dock.InitializeLayout = true;
        dock.Layout = m_layout.Root;
    }

    private void ShowContents()
    {
        IReadOnlyList<WorldEntity> contents = m_viewport.Contents();

        m_entities.Show(contents);

        Append($"world listed       {contents.Count} entities");
    }

    private static int AutoCloseSeconds()
    {
        string? given = Environment.GetEnvironmentVariable("DUNYA_SPIKE_SECONDS");

        return int.TryParse(given, out int seconds) ? seconds : 0;
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    private void Append(string line)
    {
        File.AppendAllText(m_logPath, line + Environment.NewLine);

        Dispatcher.UIThread.Post(() => m_console.Append(line));
    }
}
