using System;
using System.Runtime.InteropServices;

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
    internal static extern int dunya_session_render(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int dunya_session_extent(IntPtr session, out uint width, out uint height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr dunya_last_error();

    internal static string LastError()
    {
        IntPtr text = dunya_last_error();

        return text == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(text) ?? string.Empty;
    }
}
