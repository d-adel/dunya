namespace Dunya.Engine;

public abstract class ScriptSystem
{
    public virtual int Order => 0;

    public virtual string Name => GetType().Name;

    public abstract void Run(World world);
}
