using System.Runtime.InteropServices;

namespace Dunya.Editor;

[StructLayout(LayoutKind.Sequential)]
public struct ViewSettings
{
    public int GridVisible;
    public float Supersample;
    public int DrawMode;
    public uint FieldRepresentation;
}
