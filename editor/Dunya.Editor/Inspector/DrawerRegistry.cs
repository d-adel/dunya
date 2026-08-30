using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace Dunya.Editor.Inspector;

public static class DrawerRegistry
{
    private static readonly Dictionary<string, ComponentDrawer> Drawers = Discover();

    private static readonly DefaultDrawer Fallback = new();

    public static ComponentDrawer For(string component) =>
        Drawers.TryGetValue(component, out ComponentDrawer? drawer) ? drawer : Fallback;

    public static IReadOnlyCollection<string> Custom() => Drawers.Keys;

    private static Dictionary<string, ComponentDrawer> Discover()
    {
        var found = new Dictionary<string, ComponentDrawer>(StringComparer.Ordinal);

        foreach (Type type in Assembly.GetExecutingAssembly().GetTypes())
        {
            if (type.IsAbstract || !typeof(ComponentDrawer).IsAssignableFrom(type))
            {
                continue;
            }

            foreach (DrawerAttribute marker in type.GetCustomAttributes<DrawerAttribute>())
            {
                if (Activator.CreateInstance(type) is ComponentDrawer drawer)
                {
                    found[marker.Component] = drawer;
                }
            }
        }

        return found;
    }
}
