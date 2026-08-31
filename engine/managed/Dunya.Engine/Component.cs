namespace Dunya.Engine;

[AttributeUsage(AttributeTargets.Struct)]
public sealed class ComponentAttribute : Attribute
{
    public ComponentAttribute(string? name = null)
    {
        Name = name;
    }

    public string? Name { get; }
}
