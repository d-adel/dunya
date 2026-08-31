using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Dunya.Editor;

internal static class DunyaNative
{
    private const string Library = "dunya_c";

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_set_supersample(IntPtr session, float scale);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_show_grid(IntPtr session, int visible);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_open_world(
        IntPtr session, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_new_world(
        IntPtr session, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_save_as(
        IntPtr session, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong dunya_session_import_asset(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string file,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_package(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string runtimeExecutable,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string output,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string worlds,
        [Out] byte[]? executable,
        uint capacity,
        out uint length);

    internal static string Package(
        IntPtr session, string runtimeExecutable, string output, string[] worlds)
    {
        string joined = string.Join("\n", worlds);

        if (dunya_session_package(
                session, runtimeExecutable, output, joined, null, 0, out uint length) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        byte[] buffer = new byte[length + 1];

        if (dunya_session_package(
                session, runtimeExecutable, output, joined,
                buffer, (uint)buffer.Length, out _) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        return Encoding.UTF8.GetString(buffer, 0, (int)length);
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_worlds(
        IntPtr session, [Out] byte[]? buffer, uint capacity, out uint length);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_assets(
        IntPtr session, [Out] byte[]? buffer, uint capacity, out uint length);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_current_world(
        IntPtr session, [Out] byte[]? buffer, uint capacity, out uint length);

    private delegate int TextReader(IntPtr session, byte[]? buffer, uint capacity, out uint length);

    private static string Text(IntPtr session, TextReader read)
    {
        if (read(session, null, 0, out uint length) != 0)
        {
            return string.Empty;
        }

        if (length == 0)
        {
            return string.Empty;
        }

        byte[] buffer = new byte[length + 1];

        if (read(session, buffer, (uint)buffer.Length, out _) != 0)
        {
            return string.Empty;
        }

        return Encoding.UTF8.GetString(buffer, 0, (int)length);
    }

    internal static string[] Worlds(IntPtr session)
        => Split(Text(session, dunya_session_worlds));

    internal static string[] Assets(IntPtr session)
        => Split(Text(session, dunya_session_assets));

    internal static string CurrentWorld(IntPtr session)
        => Text(session, dunya_session_current_world);

    private static string[] Split(string joined)
        => joined.Length == 0 ? Array.Empty<string>() : joined.Split('\n');

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr dunya_session_create(
        IntPtr windowHandle,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string projectRoot,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string world);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_destroy(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_resize(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_retarget(IntPtr session, IntPtr windowHandle);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_render(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr dunya_api();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_set_log_sink(IntPtr sink);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_project_create(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string root,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_camera_orbit(IntPtr session, float yaw, float pitch);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_camera_pan(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_camera_zoom(IntPtr session, float delta);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_camera_focus(IntPtr session, uint entity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_pick(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void dunya_session_align_to_scene_camera(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_create_camera(
        IntPtr session,
        float[] position,
        float[] target,
        float verticalFov);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_create_sdf(
        IntPtr session,
        float[] position,
        float[] rotation,
        uint[] resolution,
        float margin);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_add_primitive(
        IntPtr session,
        uint entity,
        ulong material,
        IntPtr edit);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_set_static(IntPtr session, uint entity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_set_deformable(IntPtr session, uint entity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_destroy_entity(IntPtr session, uint entity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_save(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_play(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_stop(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_playing(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_material_count(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong dunya_session_material_at(IntPtr session, uint index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong dunya_session_add_material(
        IntPtr session, float[] baseColor, float metallic, float roughness
    );

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_create_light(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint dunya_session_create_environment(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr dunya_session_schedule(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr dunya_session_world(IntPtr session);


    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_extent(IntPtr session, out uint width, out uint height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_entities(
        IntPtr session,
        [Out] uint[]? entities,
        uint capacity,
        out uint count);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_entity_components(
        IntPtr session,
        uint entity,
        [Out] byte[]? buffer,
        uint capacity,
        out uint length);

    internal static uint[] Entities(IntPtr session)
    {
        if (dunya_session_entities(session, null, 0, out uint count) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        if (count == 0)
        {
            return Array.Empty<uint>();
        }

        uint[] entities = new uint[count];

        if (dunya_session_entities(session, entities, count, out _) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        return entities;
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern int dunya_session_component(
        IntPtr session,
        uint entity,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string component,
        [Out] byte[]? buffer,
        uint capacity,
        out uint length);

    internal static string? Component(IntPtr session, uint entity, string component)
    {
        if (dunya_session_component(session, entity, component, null, 0, out uint length) != 0)
        {
            return null;
        }

        if (length == 0)
        {
            return string.Empty;
        }

        byte[] buffer = new byte[length + 1];

        if (dunya_session_component(session, entity, component, buffer, (uint)buffer.Length, out _) != 0)
        {
            return null;
        }

        return Encoding.UTF8.GetString(buffer, 0, (int)length);
    }
    internal static string[] Components(IntPtr session, uint entity)
    {
        if (dunya_session_entity_components(session, entity, null, 0, out uint length) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        if (length == 0)
        {
            return Array.Empty<string>();
        }

        byte[] buffer = new byte[length + 1];

        if (dunya_session_entity_components(session, entity, buffer, (uint)buffer.Length, out _) != 0)
        {
            throw new InvalidOperationException(LastError());
        }

        return Encoding.UTF8.GetString(buffer, 0, (int)length).Split('\n');
    }
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr dunya_last_error();

    internal static string LastError()
    {
        IntPtr text = dunya_last_error();

        return text == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(text) ?? string.Empty;
    }
}
