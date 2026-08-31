using System.Runtime.InteropServices;

namespace Dunya.Engine;

[StructLayout(LayoutKind.Sequential)]
public struct Pose
{
    public Vector3 Position;
    public Quaternion Rotation;
}
