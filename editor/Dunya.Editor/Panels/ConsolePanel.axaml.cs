using System;
using System.Text;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public partial class ConsolePanel : UserControl
{
    private readonly StringBuilder m_lines = new();

    public ConsolePanel() => InitializeComponent();

    public void Append(string line)
    {
        m_lines.AppendLine(line);

        this.FindControl<SelectableTextBlock>("Text")!.Text = m_lines.ToString();
        this.FindControl<ScrollViewer>("Scroller")!.ScrollToEnd();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
