using System;
using System.IO;
using System.Reflection;

namespace Dunya.Editor;

public sealed class Scripts
{
    private MethodInfo? m_start;

    public string? Failure { get; private set; }

    public bool Loaded => m_start != null;

    public bool Load(string managed)
    {
        string path = Path.Combine(managed, "Dunya.Engine.dll");

        if (!File.Exists(path))
        {
            Failure = "No managed engine assembly at " + path;
            return false;
        }

        try
        {
            Assembly assembly = Assembly.LoadFrom(path);

            Type? boot = assembly.GetType("Dunya.Engine.Boot");

            if (boot == null)
            {
                Failure = "The managed engine assembly has no Dunya.Engine.Boot";
                return false;
            }

            m_start = boot.GetMethod(
                "Start", BindingFlags.Public | BindingFlags.Static
            );

            if (m_start == null)
            {
                Failure = "The managed engine assembly has no Boot.Start";
                return false;
            }
        }
        catch (Exception error)
        {
            Failure = error.Message;
            return false;
        }

        return true;
    }

    public bool Start(IntPtr api, IntPtr schedule, IntPtr world, string scripts)
    {
        if (m_start == null)
        {
            Failure = "The managed engine assembly is not loaded";
            return false;
        }

        try
        {
            object? result =
                m_start.Invoke(null, new object[] { api, schedule, world, scripts });

            if (result is int code && code == 1)
            {
                return true;
            }

            Failure = result is int refused && refused == 2
                ? "The scripts refused the engine api: it is a different version than they were built against"
                : "The scripts refused to start";
        }
        catch (Exception error)
        {
            Failure = error.InnerException?.Message ?? error.Message;
        }

        return false;
    }
}
