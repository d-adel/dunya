using System;
using System.Runtime.InteropServices;

namespace Dunya.Editor;

internal static class ScriptLog
{
    internal static Action<string>? Reported;

    internal static void Attach()
    {
        unsafe
        {
            delegate* unmanaged<byte*, void> sink = &Receive;

            DunyaNative.dunya_set_log_sink((IntPtr)sink);
        }
    }

    [UnmanagedCallersOnly]
    private static unsafe void Receive(byte* message)
    {
        if (message == null)
        {
            return;
        }

        string? text = Marshal.PtrToStringUTF8((IntPtr)message);

        if (text != null)
        {
            Reported?.Invoke("[script] " + text);
        }
    }
}
