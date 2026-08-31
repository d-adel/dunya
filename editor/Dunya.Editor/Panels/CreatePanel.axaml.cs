using System;
using System.Collections.Generic;
using System.Globalization;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Dunya.Editor.Panels;

public partial class CreatePanel : UserControl
{
    public CreatePanel()
    {
        AvaloniaXamlLoader.Load(this);

        this.FindControl<Button>("CreateEntity")!.Click +=
            (_, _) => Create?.Invoke(Read());

        this.FindControl<Button>("AddToSelected")!.Click +=
            (_, _) => AddShape?.Invoke(Read(), false);

        this.FindControl<Button>("SubtractFromSelected")!.Click +=
            (_, _) => AddShape?.Invoke(Read(), true);

        this.FindControl<Button>("MakeMaterial")!.Click +=
            (_, _) => CreateMaterial?.Invoke(ReadMaterial());

        foreach (string name in new[]
                 {
                     "SizeX", "SizeY", "SizeZ",
                     "ChunksX", "ChunksY", "ChunksZ",
                     "Resolution", "GridMargin"
                 })
        {
            this.FindControl<TextBox>(name)!.TextChanged += (_, _) => ShowVoxelSize();
        }

        ShowVoxelSize();
    }

    public event Action<ShapeRequest>? Create;

    public event Action<ShapeRequest, bool>? AddShape;

    public event Action<MaterialRequest>? CreateMaterial;

    public void ShowMaterials(IReadOnlyList<string> names, int selected)
    {
        ComboBox box = this.FindControl<ComboBox>("Material")!;

        box.ItemsSource = names;
        box.SelectedIndex = names.Count == 0 ? -1 : Math.Clamp(selected, 0, names.Count - 1);
    }

    public uint SelectedMaterial()
    {
        int index = this.FindControl<ComboBox>("Material")?.SelectedIndex ?? 0;

        return (uint)Math.Max(index, 0);
    }

    private void ShowVoxelSize()
    {
        ShapeRequest request = Read();

        float span = 2.0f * Math.Max(request.SizeX / request.ChunksX, 0.0f)
                     + 2.0f * request.Margin;

        float voxel = span / Math.Max(request.Resolution - 1u, 1u);

        this.FindControl<TextBlock>("VoxelSize")!.Text =
            string.Format(
                CultureInfo.InvariantCulture,
                "{0} chunk(s), voxel {1:F1} mm",
                request.ChunksX * request.ChunksY * request.ChunksZ,
                voxel * 1000.0f
            );
    }

    private float Number(string name, float fallback)
    {
        string text = this.FindControl<TextBox>(name)?.Text ?? string.Empty;

        return float.TryParse(
            text, NumberStyles.Float, CultureInfo.InvariantCulture, out float held
        )
            ? held
            : fallback;
    }

    private uint Count(string name)
        => (uint)Math.Clamp(Number(name, 1.0f), 1.0f, 32.0f);

    private MaterialRequest ReadMaterial() => new()
    {
        R = Number("ColorR", 0.6f),
        G = Number("ColorG", 0.6f),
        B = Number("ColorB", 0.6f),
        Metallic = Number("Metallic", 0.0f),
        Roughness = Number("Roughness", 0.8f)
    };

    private ShapeRequest Read() => new()
    {
        Kind = (uint)(this.FindControl<ComboBox>("Shape")?.SelectedIndex ?? 0) switch
        {
            1u => Authoring.Sphere,
            2u => Authoring.Cylinder,
            _ => Authoring.Box
        },
        X = Number("PosX", 0.0f),
        Y = Number("PosY", 0.0f),
        Z = Number("PosZ", 0.0f),
        SizeX = Number("SizeX", 0.5f),
        SizeY = Number("SizeY", 0.5f),
        SizeZ = Number("SizeZ", 0.5f),
        ChunksX = Count("ChunksX"),
        ChunksY = Count("ChunksY"),
        ChunksZ = Count("ChunksZ"),
        Margin = Math.Max(Number("GridMargin", 0.5f), 0.0f),
        Static = this.FindControl<CheckBox>("IsStatic")?.IsChecked == true,
        Deformable = this.FindControl<CheckBox>("IsDeformable")?.IsChecked == true,
        Resolution = (uint)Math.Clamp(Number("Resolution", 65.0f), 9.0f, 129.0f)
    };
}

public struct ShapeRequest
{
    public uint Kind;
    public float X;
    public float Y;
    public float Z;
    public float SizeX;
    public float SizeY;
    public float SizeZ;
    public uint ChunksX;
    public uint ChunksY;
    public uint ChunksZ;
    public float Margin;
    public uint Resolution;
    public bool Static;
    public bool Deformable;
}

public struct MaterialRequest
{
    public float R;
    public float G;
    public float B;
    public float Metallic;
    public float Roughness;
}
