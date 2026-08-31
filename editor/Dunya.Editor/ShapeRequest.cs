namespace Dunya.Editor;

public sealed class ShapeRequest
{
    public uint Kind { get; set; }

    public float X { get; set; }

    public float Y { get; set; }

    public float Z { get; set; }

    public float SizeX { get; set; } = 0.5f;

    public float SizeY { get; set; } = 0.5f;

    public float SizeZ { get; set; } = 0.5f;

    public uint ChunksX { get; set; } = 1;

    public uint ChunksY { get; set; } = 1;

    public uint ChunksZ { get; set; } = 1;

    public uint Resolution { get; set; } = 65;

    public float Margin { get; set; } = 0.5f;

    public bool Static { get; set; }

    public bool Deformable { get; set; } = true;
}
