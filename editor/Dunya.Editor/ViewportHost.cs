using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;

namespace Dunya.Editor;

public sealed class ViewportHost : NativeControlHost
{
    private const int GWLP_WNDPROC = -4;

    private const uint WM_SIZE = 0x0005;
    private const uint WM_SETFOCUS = 0x0007;
    private const uint WM_KEYDOWN = 0x0100;
    private const uint WM_MOUSEMOVE = 0x0200;
    private const uint WM_LBUTTONDOWN = 0x0201;
    private const uint WM_RBUTTONDOWN = 0x0204;
    private const uint WM_MOUSEWHEEL = 0x020A;

    private delegate IntPtr WindowProc(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    private static extern IntPtr SetWindowLongPtr(IntPtr window, int index, IntPtr value);

    [DllImport("user32.dll", EntryPoint = "CallWindowProcW")]
    private static extern IntPtr CallWindowProc(IntPtr previous, IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr window);

    public event Action<string>? Reported;

    public event Action? WorldOpened;

    public IntPtr Handle { get; private set; }

    public string ProjectRoot { get; set; } = "projects/demo";

    public string World { get; set; } = "main";

    private bool m_surfaceLive;
    private IntPtr m_session;
    private DispatcherTimer? m_frames;
    private bool m_renderFailed;
    private IntPtr m_previous;
    private WindowProc? m_hook;
    private int m_moves;

    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        IPlatformHandle handle = base.CreateNativeControlCore(parent);

        Handle = handle.Handle;

        m_hook = Intercept;
        m_previous = SetWindowLongPtr(
            Handle,
            GWLP_WNDPROC,
            Marshal.GetFunctionPointerForDelegate(m_hook)
        );

        Report($"child window created  hwnd=0x{Handle:X}  descriptor={handle.HandleDescriptor}");

        if (m_session == IntPtr.Zero)
        {
            StartSession();
        }
        else
        {
            Retarget();
        }

        return handle;
    }


    public void Shutdown() => StopSession();

    private void Retarget()
    {
        if (DunyaNative.dunya_session_retarget(m_session, Handle) != 0)
        {
            Report($"session retarget FAILED: {DunyaNative.LastError()}");

            return;
        }

        m_surfaceLive = true;

        Report($"session retargeted  hwnd=0x{Handle:X}   {Extent()}");

        StartRendering();
    }

    protected override void DestroyNativeControlCore(IPlatformHandle control)
    {
        m_surfaceLive = false;

        m_frames?.Stop();
        m_frames = null;

        if (m_previous != IntPtr.Zero)
        {
            SetWindowLongPtr(Handle, GWLP_WNDPROC, m_previous);
            m_previous = IntPtr.Zero;
        }

        Handle = IntPtr.Zero;
        m_hook = null;

        base.DestroyNativeControlCore(control);
    }

    private void StartSession()
    {
        m_session = DunyaNative.dunya_session_create(Handle, ProjectRoot, World);

        if (m_session == IntPtr.Zero)
        {
            Report($"session create FAILED: {DunyaNative.LastError()}");

            return;
        }

        m_surfaceLive = true;

        Report($"session created    handle=0x{m_session:X}   {Extent()}");

        StartRendering();

        WorldOpened?.Invoke();
    }

    private void StopSession()
    {
        if (m_session == IntPtr.Zero)
        {
            return;
        }

        m_surfaceLive = false;

        m_frames?.Stop();
        m_frames = null;

        DunyaNative.dunya_session_destroy(m_session);
        m_session = IntPtr.Zero;

        Report("session destroyed");
    }

    private void StartRendering()
    {
        m_frames?.Stop();

        m_frames = new DispatcherTimer(DispatcherPriority.Render)
        {
            Interval = TimeSpan.FromMilliseconds(16)
        };

        m_frames.Tick += (_, _) => RenderOnce();
        m_frames.Start();
    }

    private void RenderOnce()
    {
        if (m_session == IntPtr.Zero || m_renderFailed || !m_surfaceLive)
        {
            return;
        }

        if (DunyaNative.dunya_session_render(m_session) != 0)
        {
            m_renderFailed = true;

            Report($"render FAILED: {DunyaNative.LastError()}");
        }
    }

    public IReadOnlyList<WorldEntity> Contents()
    {
        if (m_session == IntPtr.Zero)
        {
            return Array.Empty<WorldEntity>();
        }

        try
        {
            uint[] ids = DunyaNative.Entities(m_session);
            var contents = new List<WorldEntity>(ids.Length);

            foreach (uint id in ids)
            {
                contents.Add(new WorldEntity(id, DunyaNative.Components(m_session, id)));
            }

            return contents;
        }
        catch (InvalidOperationException failure)
        {
            Report($"contents FAILED: {failure.Message}");

            return Array.Empty<WorldEntity>();
        }
    }

    public string? Component(uint entity, string component)
    {
        if (m_session == IntPtr.Zero)
        {
            return null;
        }

        return DunyaNative.Component(m_session, entity, component);
    }
    internal string Extent()
    {
        if (m_session == IntPtr.Zero)
        {
            return "no session";
        }

        if (DunyaNative.dunya_session_extent(m_session, out uint width, out uint height) != 0)
        {
            return $"extent FAILED: {DunyaNative.LastError()}";
        }

        return $"swapchain {width} x {height}";
    }

    private IntPtr Intercept(IntPtr window, uint message, IntPtr wParam, IntPtr lParam)
    {
        switch (message)
        {
            case WM_SIZE:
                if (m_session != IntPtr.Zero && !m_surfaceLive && Handle != IntPtr.Zero)
                {
                    Retarget();
                }
                else if (m_session != IntPtr.Zero
                    && m_surfaceLive
                    && DunyaNative.dunya_session_resize(m_session) != 0)
                {
                    Report($"resize FAILED: {DunyaNative.LastError()}");
                }

                Report(
                    $"WM_SIZE          client {Low(lParam)} x {High(lParam)}"
                    + $"   {Extent()}"
                    + $"   scale {(TopLevel.GetTopLevel(this)?.RenderScaling ?? 0.0):F2}"
                );
                break;

            case WM_SETFOCUS:
                Report("WM_SETFOCUS      the child window has keyboard focus");
                break;

            case WM_MOUSEMOVE:
                if (++m_moves % 30 == 1)
                {
                    Report($"WM_MOUSEMOVE     ({Low(lParam)}, {High(lParam)})   [1 of every 30]");
                }
                break;

            case WM_LBUTTONDOWN:
                Report($"WM_LBUTTONDOWN   ({Low(lParam)}, {High(lParam)})  -> asking for focus");
                SetFocus(window);
                break;

            case WM_RBUTTONDOWN:
                Report($"WM_RBUTTONDOWN   ({Low(lParam)}, {High(lParam)})");
                break;

            case WM_MOUSEWHEEL:
                Report($"WM_MOUSEWHEEL    delta {(short)(wParam.ToInt64() >> 16)}");
                break;

            case WM_KEYDOWN:
                Report($"WM_KEYDOWN       virtual key {wParam.ToInt64()}");
                break;
        }

        return CallWindowProc(m_previous, window, message, wParam, lParam);
    }

    private static int Low(IntPtr value) => (short)(value.ToInt64() & 0xFFFF);

    private static int High(IntPtr value) => (short)((value.ToInt64() >> 16) & 0xFFFF);

    private void Report(string line) => Reported?.Invoke(line);
}
