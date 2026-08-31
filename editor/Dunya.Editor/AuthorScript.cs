using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;
using Dunya.Editor.Panels;

namespace Dunya.Editor;

public sealed class AuthorMaterial
{
    public float R { get; set; }
    public float G { get; set; }
    public float B { get; set; }
    public float Metallic { get; set; }
    public float Roughness { get; set; } = 0.8f;
}

public sealed class AuthorStep
{
    public string Op { get; set; } = "create";
    public string Kind { get; set; } = "box";
    public bool Subtract { get; set; }
    public float X { get; set; }
    public float Y { get; set; }
    public float Z { get; set; }
    public float Sx { get; set; } = 0.5f;
    public float Sy { get; set; } = 0.5f;
    public float Sz { get; set; } = 0.5f;
    public uint ChunksX { get; set; } = 1;
    public uint ChunksY { get; set; } = 1;
    public uint ChunksZ { get; set; } = 1;
    public uint Resolution { get; set; } = 65;
    public float Margin { get; set; } = 0.5f;
    public uint Material { get; set; }
    public bool Static { get; set; }
    public bool Deformable { get; set; }
    public float[]? At { get; set; }
    public float[]? Target { get; set; }
    public float Fov { get; set; } = 55.0f;
}

public sealed class AuthorScript
{
    public List<AuthorMaterial> Materials { get; set; } = new();

    public List<AuthorStep> Steps { get; set; } = new();

    public static AuthorScript? Read(string path)
    {
        if (!File.Exists(path))
        {
            return null;
        }

        return JsonSerializer.Deserialize<AuthorScript>(
            File.ReadAllText(path),
            new JsonSerializerOptions { PropertyNameCaseInsensitive = true }
        );
    }

    public static uint KindOf(string named) => named.ToLowerInvariant() switch
    {
        "sphere" => Authoring.Sphere,
        "cylinder" => Authoring.Cylinder,
        "plane" => Authoring.Plane,
        _ => Authoring.Box
    };

    public static ShapeRequest RequestOf(AuthorStep step) => new()
    {
        Kind = KindOf(step.Kind),
        X = step.X,
        Y = step.Y,
        Z = step.Z,
        SizeX = step.Sx,
        SizeY = step.Sy,
        SizeZ = step.Sz,
        ChunksX = Math.Max(step.ChunksX, 1u),
        ChunksY = Math.Max(step.ChunksY, 1u),
        ChunksZ = Math.Max(step.ChunksZ, 1u),
        Margin = step.Margin,
        Resolution = step.Resolution
    };

    public static bool PositionOf(string? poseJson, out float[] position)
    {
        position = new float[3];

        if (string.IsNullOrEmpty(poseJson))
        {
            return false;
        }

        using JsonDocument document = JsonDocument.Parse(poseJson);

        if (!document.RootElement.TryGetProperty("position", out JsonElement held)
            || held.GetArrayLength() != 3)
        {
            return false;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            position[axis] = held[axis].GetSingle();
        }

        return true;
    }

    public static string Describe(float[] at)
        => string.Format(
            CultureInfo.InvariantCulture, "({0}, {1}, {2})", at[0], at[1], at[2]
        );
}
