using System.Text;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public partial class ConsolePanel : UserControl
{
    private readonly StringBuilder m_editor = new();
    private readonly StringBuilder m_game = new();

    public ConsolePanel() => InitializeComponent();

    public void Append(string line)
    {
        m_editor.AppendLine(line);

        this.FindControl<SelectableTextBlock>("EditorText")!.Text =
            m_editor.ToString();
        this.FindControl<ScrollViewer>("EditorScroller")!.ScrollToEnd();
    }

    public void AppendGame(string line)
    {
        m_game.AppendLine(line);

        this.FindControl<SelectableTextBlock>("GameText")!.Text =
            m_game.ToString();
        this.FindControl<ScrollViewer>("GameScroller")!.ScrollToEnd();
    }

    public void ClearGame()
    {
        m_game.Clear();

        this.FindControl<SelectableTextBlock>("GameText")!.Text = string.Empty;
    }

    public void ShowGame(bool playing)
    {
        this.FindControl<TabControl>("Tabs")!.SelectedIndex = playing ? 1 : 0;
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
}
