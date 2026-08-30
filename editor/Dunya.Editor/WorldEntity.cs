using System.Linq;

namespace Dunya.Editor;

public sealed class WorldEntity
{
    public WorldEntity(uint id, string[] components)
    {
        Id = id;
        Components = components;
    }

    public uint Id { get; }

    public string[] Components { get; }

    public string Label => $"{Id,4}   {Kind}";

    public string Summary => string.Join("  ", Components);

    private string Kind =>
        Components.Contains("SdfGrid") ? "SdfGrid"
        : Components.Contains("Mesh") ? "Mesh"
        : "Entity";
}
