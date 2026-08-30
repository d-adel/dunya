using Avalonia.Controls;
using Dock.Model.Avalonia;
using Dock.Model.Avalonia.Controls;
using Dock.Model.Core;

namespace Dunya.Editor.Shell;

public sealed class EditorLayout
{
    private EditorLayout(
        RootDock root,
        Tool entities,
        Tool viewport,
        Tool inspector,
        Tool console)
    {
        Root = root;
        Entities = entities;
        Viewport = viewport;
        Inspector = inspector;
        Console = console;
    }

    public RootDock Root { get; }

    public Tool Entities { get; }

    public Tool Viewport { get; }

    public Tool Inspector { get; }

    public Tool Console { get; }

    public static EditorLayout Build(
        Factory factory,
        Control entities,
        Control viewport,
        Control inspector,
        Control console)
    {
        Tool entitiesTool = Panel("Entities", entities);
        Tool viewportTool = Panel("Viewport", viewport);
        Tool inspectorTool = Panel("Inspector", inspector);
        Tool consoleTool = Panel("Console", console);


        ToolDock left = Holder(factory, "EntitiesDock", Alignment.Left, 0.18, entitiesTool);
        ToolDock centre = Holder(factory, "ViewportDock", Alignment.Unset, double.NaN, viewportTool);
        ToolDock bottom = Holder(factory, "ConsoleDock", Alignment.Bottom, 0.28, consoleTool);
        ToolDock right = Holder(factory, "InspectorDock", Alignment.Right, 0.22, inspectorTool);

        var middle = new ProportionalDock
        {
            Id = "MiddleColumn",
            Title = "MiddleColumn",
            Orientation = Orientation.Vertical,
            VisibleDockables = factory.CreateList<IDockable>(
                centre,
                new ProportionalDockSplitter(),
                bottom)
        };

        var main = new ProportionalDock
        {
            Id = "MainLayout",
            Title = "MainLayout",
            Orientation = Orientation.Horizontal,
            VisibleDockables = factory.CreateList<IDockable>(
                left,
                new ProportionalDockSplitter(),
                middle,
                new ProportionalDockSplitter(),
                right)
        };

        var root = new RootDock
        {
            Id = "Root",
            Title = "Root",
            IsCollapsable = false,
            VisibleDockables = factory.CreateList<IDockable>(main),
            ActiveDockable = main,
            DefaultDockable = main
        };

        return new EditorLayout(root, entitiesTool, viewportTool, inspectorTool, consoleTool);
    }

    private static Tool Panel(string name, Control content) => new()
    {
        Id = name,
        Title = name,
        Content = content,
        CanClose = false
    };

    private static ToolDock Holder(
        Factory factory,
        string id,
        Alignment alignment,
        double proportion,
        Tool tool)
    {
        return new ToolDock
        {
            Id = id,
            Title = id,
            Alignment = alignment,
            Proportion = proportion,
            VisibleDockables = factory.CreateList<IDockable>(tool),
            ActiveDockable = tool
        };
    }
}
