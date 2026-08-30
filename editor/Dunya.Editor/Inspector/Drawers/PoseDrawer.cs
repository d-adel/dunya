using Dunya.Editor.Inspector;

namespace Dunya.Editor.Inspector.Drawers;

[Drawer("Pose")]
public sealed class PoseDrawer : ComponentDrawer
{
    protected override void Build(ComponentBuilder ui)
    {
        ui.Vector("Position", "position", "m");
        ui.Euler("Rotation", "rotation");
    }
}
