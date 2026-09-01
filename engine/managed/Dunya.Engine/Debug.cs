namespace Dunya.Engine;

public static class Debug
{
    public static void Log(string message) => Native.Log(message);
}
