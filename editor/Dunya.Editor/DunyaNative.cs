using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Dunya.Editor;

internal static class DunyaNative
{
    private const string Library = "dunya_c";

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
