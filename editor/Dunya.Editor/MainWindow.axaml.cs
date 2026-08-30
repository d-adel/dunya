using System;
using System.IO;
using System.Text;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;

namespace Dunya.Editor;

public partial class MainWindow : Window
{
    private readonly StringBuilder m_log = new();

    private readonly string m_logPath =
        Path.Combine(AppContext.BaseDirectory, "spike.log");

    private bool m_sawAvaloniaPointer;

    public MainWindow()
    {
        InitializeComponent();

        File.WriteAllText(m_logPath, string.Empty);

        ViewportHost viewport = this.FindControl<ViewportHost>("Viewport")!;

        viewport.Reported += Append;

        viewport.PointerMoved += (_, _) =>
            AppendOnce("avalonia PointerMoved reached the HOST CONTROL");

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

    private static int AutoCloseSeconds()
    {
        string? given = Environment.GetEnvironmentVariable("DUNYA_SPIKE_SECONDS");

        return int.TryParse(given, out int seconds) ? seconds : 0;
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    private void AppendOnce(string line)
    {
        if (m_sawAvaloniaPointer)
        {
            return;
        }

        m_sawAvaloniaPointer = true;

        Append(line);
    }

    private void Append(string line)
    {
        File.AppendAllText(m_logPath, line + Environment.NewLine);

        Dispatcher.UIThread.Post(() =>
        {
            m_log.AppendLine(line);

            this.FindControl<SelectableTextBlock>("Log")!.Text = m_log.ToString();
            this.FindControl<ScrollViewer>("LogScroller")!.ScrollToEnd();
        });
    }
}
