using System;
using System.Collections.Generic;
using System.IO;
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
    private const uint WM_LBUTTONUP = 0x0202;
    private const uint WM_RBUTTONUP = 0x0205;
    private const uint WM_MBUTTONDOWN = 0x0207;
    private const uint WM_MBUTTONUP = 0x0208;
    private const uint WM_MOUSEWHEEL = 0x020A;

    private const long MK_ALT_HELD = 0x0020;
    private const long VK_F = 0x46;

    private const float OrbitRate = 0.008f;
    private const float PanRate = 0.0016f;

    private enum Drag
    {
        None,
        Select,
        Orbit,
        Pan
    }

    private delegate IntPtr WindowProc(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    private static extern IntPtr SetWindowLongPtr(IntPtr window, int index, IntPtr value);

    [DllImport("user32.dll", EntryPoint = "CallWindowProcW")]
    private static extern IntPtr CallWindowProc(IntPtr previous, IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr SetCapture(IntPtr window);

    [DllImport("user32.dll")]
    private static extern bool ReleaseCapture();

    public event Action<string>? Reported;

    public event Action? WorldOpened;

    public event Action<uint?>? Picked;

    public event Action? FocusRequested;

    public IntPtr Handle { get; private set; }

    public string ProjectRoot { get; set; } = "projects/demo";

    public string World { get; set; } = "main";

    private bool m_surfaceLive;
    private IntPtr m_session;
    private DispatcherTimer? m_frames;
    private bool m_renderFailed;
    private IntPtr m_previous;
    private WindowProc? m_hook;
    private Drag m_drag = Drag.None;
    private int m_dragX;
    private int m_dragY;
    private bool m_dragged;

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


    public IntPtr SessionHandle => m_session;

    public void Shutdown() => StopSession();

    public void Reopen(string projectRoot, string world)
    {
        StopSession();

        ProjectRoot = projectRoot;
        World = world;

        if (Handle != IntPtr.Zero)
        {
            StartSession();
        }
    }

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

        StartScripts();

        StartRendering();

        WorldOpened?.Invoke();
    }


    private readonly Scripts m_scripts = new();

    private void StartScripts()
    {
        string managed = Path.Combine(Environment.CurrentDirectory, "managed");

        if (!m_scripts.Load(managed))
        {
            Report($"scripts NOT loaded: {m_scripts.Failure}");

            return;
        }

        ScriptLog.Reported = Report;
        ScriptLog.Attach();

        IntPtr api = DunyaNative.dunya_api();
        IntPtr schedule = DunyaNative.dunya_session_schedule(m_session);
        IntPtr world = DunyaNative.dunya_session_world(m_session);

        string directory = Path.Combine(
            Environment.CurrentDirectory, ProjectRoot, "scripts"
        );

        if (!m_scripts.Start(api, schedule, world, directory))
        {
            Report($"scripts NOT started: {m_scripts.Failure}");

            return;
        }

        Report("scripts started");
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
                break;

            case WM_LBUTTONDOWN:
                SetFocus(window);
                m_dragX = Low(lParam);
                m_dragY = High(lParam);
                m_dragged = false;

                if ((wParam.ToInt64() & MK_ALT_HELD) != 0)
                {
                    m_drag = Drag.Orbit;
                    SetCapture(window);
                }
                else
                {
                    m_drag = Drag.Select;
                }
                break;

            case WM_RBUTTONDOWN:
                SetFocus(window);
                m_dragX = Low(lParam);
                m_dragY = High(lParam);
                m_drag = Drag.Orbit;
                SetCapture(window);
                break;

            case WM_MBUTTONDOWN:
                SetFocus(window);
                m_dragX = Low(lParam);
                m_dragY = High(lParam);
                m_drag = Drag.Pan;
                SetCapture(window);
                break;

            case WM_MOUSEMOVE:
            {
                if (m_drag == Drag.None || m_session == IntPtr.Zero)
                {
                    break;
                }

                int x = Low(lParam);
                int y = High(lParam);

                float dx = x - m_dragX;
                float dy = y - m_dragY;

                m_dragX = x;
                m_dragY = y;

                if (dx != 0.0f || dy != 0.0f)
                {
                    m_dragged = true;
                }

                if (m_drag == Drag.Orbit)
                {
                    DunyaNative.dunya_session_camera_orbit(
                        m_session, dx * OrbitRate, -dy * OrbitRate
                    );
                }
                else if (m_drag == Drag.Pan)
                {
                    DunyaNative.dunya_session_camera_pan(
                        m_session, dx * PanRate, dy * PanRate
                    );
                }
                break;
            }

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                if (m_drag == Drag.Select && !m_dragged && m_session != IntPtr.Zero)
                {
                    uint hit = DunyaNative.dunya_session_pick(
                        m_session, Low(lParam), High(lParam)
                    );

                    Picked?.Invoke(hit == uint.MaxValue ? null : hit);
                }

                if (m_drag != Drag.None && m_drag != Drag.Select)
                {
                    ReleaseCapture();
                }

                m_drag = Drag.None;
                break;

            case WM_MOUSEWHEEL:
                if (m_session != IntPtr.Zero)
                {
                    DunyaNative.dunya_session_camera_zoom(
                        m_session, (short)(wParam.ToInt64() >> 16) / 120.0f
                    );
                }
                break;

            case WM_KEYDOWN:
                if (m_session != IntPtr.Zero && wParam.ToInt64() == VK_F)
                {
                    FocusRequested?.Invoke();
                }
                break;
        }

        return CallWindowProc(m_previous, window, message, wParam, lParam);
    }
    private static int Low(IntPtr value) => (short)(value.ToInt64() & 0xFFFF);

    private static int High(IntPtr value) => (short)((value.ToInt64() >> 16) & 0xFFFF);

    private void Report(string line) => Reported?.Invoke(line);
}
