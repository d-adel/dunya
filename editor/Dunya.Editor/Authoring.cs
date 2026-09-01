using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace Dunya.Editor;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct SdfEditDescriptor
{
    public uint Kind;
    public uint Material;
    public uint Operation;
    public float Blend;
    public fixed float Position[3];
    public fixed float Size[3];
    public fixed float Rotation[4];
}

public sealed class Authoring
{
    public const uint Sphere = 0;
    public const uint Box = 1;
    public const uint Plane = 2;
    public const uint Cylinder = 3;

    public const uint Union = 0;
    public const uint Subtract = 3;

    private readonly IntPtr m_session;

    public Authoring(IntPtr session)
    {
        m_session = session;
    }

    public uint CreateSdf(float x, float y, float z, uint resolution, float margin)
    {
        return DunyaNative.dunya_session_create_sdf(
            m_session,
            new[] { x, y, z },
            new[] { 0.0f, 0.0f, 0.0f, 1.0f },
            new[] { resolution, resolution, resolution },
            margin
        );
    }

    public uint CreateLight()
        => DunyaNative.dunya_session_create_light(m_session);

    public uint CreateEnvironment()
        => DunyaNative.dunya_session_create_environment(m_session);

    public void AlignToSceneCamera()
        => DunyaNative.dunya_session_align_to_scene_camera(m_session);

    public void FocusCamera(uint? entity)
        => DunyaNative.dunya_session_camera_focus(m_session, entity ?? uint.MaxValue);

    public uint CreateCamera(
        float x, float y, float z, float tx, float ty, float tz, float fov
    )
        => DunyaNative.dunya_session_create_camera(
            m_session, new[] { x, y, z }, new[] { tx, ty, tz }, fov
        );

    public unsafe bool AddPrimitive(
        uint entity,
        uint kind,
        uint operation,
        ulong material,
        float x,
        float y,
        float z,
        float sx,
        float sy,
        float sz
    )
    {
        SdfEditDescriptor edit = default;

        edit.Kind = kind;
        edit.Operation = operation;
        edit.Position[0] = x;
        edit.Position[1] = y;
        edit.Position[2] = z;
        edit.Size[0] = sx;
        edit.Size[1] = sy;
        edit.Size[2] = sz;
        edit.Rotation[3] = 1.0f;

        return DunyaNative.dunya_session_add_primitive(
            m_session, entity, material, (IntPtr)(&edit)
        ) == 0;
    }

    public bool SetStatic(uint entity)
        => DunyaNative.dunya_session_set_static(m_session, entity) == 0;

    public bool SetDeformable(uint entity)
        => DunyaNative.dunya_session_set_deformable(m_session, entity) == 0;

    public bool Destroy(uint entity)
        => DunyaNative.dunya_session_destroy_entity(m_session, entity) == 0;

    public bool Save() => DunyaNative.dunya_session_save(m_session) == 0;

    public void SetSupersample(float scale)
        => DunyaNative.dunya_session_set_supersample(m_session, scale);

    public float Supersample() => DunyaNative.dunya_session_supersample(m_session);

    public void ShowGrid(bool visible)
        => DunyaNative.dunya_session_show_grid(m_session, visible ? 1 : 0);

    public bool OpenWorld(string name)
        => DunyaNative.dunya_session_open_world(m_session, name) == 0;

    public bool NewWorld(string name)
        => DunyaNative.dunya_session_new_world(m_session, name) == 0;

    public bool SaveAs(string name)
        => DunyaNative.dunya_session_save_as(m_session, name) == 0;

    public string[] Worlds() => DunyaNative.Worlds(m_session);

    public string[] Assets() => DunyaNative.Assets(m_session);

    public string CurrentWorld() => DunyaNative.CurrentWorld(m_session);

    public ulong ImportAsset(string file, string type)
        => DunyaNative.dunya_session_import_asset(m_session, file, type);

    public static string RuntimeExecutable()
    {
        string beside = Path.Combine(
            AppContext.BaseDirectory, "DunyaRuntime.exe");

        return File.Exists(beside)
            ? beside
            : Path.Combine(Directory.GetCurrentDirectory(), "DunyaRuntime.exe");
    }

    public string Package(string output, string[] worlds)
        => DunyaNative.Package(m_session, RuntimeExecutable(), output, worlds);

    public bool Play() => DunyaNative.dunya_session_play(m_session) == 0;

    public bool Stop() => DunyaNative.dunya_session_stop(m_session) == 0;

    public bool Playing() => DunyaNative.dunya_session_playing(m_session) != 0;

    public uint MaterialCount()
        => DunyaNative.dunya_session_material_count(m_session);

    public ulong MaterialAt(uint index)
        => DunyaNative.dunya_session_material_at(m_session, index);

    public ulong AddMaterial(
        float r, float g, float b, float metallic, float roughness
    )
        => DunyaNative.dunya_session_add_material(
            m_session, new[] { r, g, b, 1.0f }, metallic, roughness
        );

    public ulong DefaultMaterial()
        => MaterialCount() == 0 ? 0ul : MaterialAt(0);

    public static bool CreateProject(string root, string name)
        => DunyaNative.dunya_project_create(root, name) == 0;
}
