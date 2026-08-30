using System;
using Avalonia.Controls;

namespace Dunya.Editor.Inspector;

[AttributeUsage(AttributeTargets.Class)]
public sealed class DrawerAttribute : Attribute
{
    public DrawerAttribute(string component) => Component = component;

    public string Component { get; }
}

public abstract class ComponentDrawer
{
    protected abstract void Build(ComponentBuilder ui);

    public Control Draw(ComponentBuilder ui)
    {
        Build(ui);

        return ui.Build();
    }
}

public sealed class DefaultDrawer : ComponentDrawer
{
    protected override void Build(ComponentBuilder ui) => ui.Auto();
}
