using Dunya.Engine;

namespace Puzzle;

public enum Tool : uint
{
    Brush = 0,
    Chisel = 1,
    Saw = 2,
    Drill = 3
}

[Component]
public struct CarveRequest
{
    public uint Tool;
    public Vector3 Position;
    public Vector3 Size;
}

public sealed class CarveSystem : ScriptSystem
{
    private static float s_spent;

    public override int Order => Phases.Excavate;

    public override string Name => "carve";

    public static float Spent => s_spent;

    public static void Forget() => s_spent = 0.0f;

    public static void Reach(
        Tool tool, Vector3 at, Vector3 size, out Vector3 low, out Vector3 high
    )
    {
        Vector3 half = tool switch
        {
            Tool.Chisel => size,
            Tool.Drill => new Vector3(size.X, size.Y, size.X),
            Tool.Saw => new Vector3(64.0f, 64.0f, 64.0f),
            _ => new Vector3(size.X, size.X, size.X)
        };

        low = new Vector3(at.X - half.X, at.Y - half.Y, at.Z - half.Z);
        high = new Vector3(at.X + half.X, at.Y + half.Y, at.Z + half.Z);
    }

    public override void Run(World world)
    {
        if (world.Count<CarveRequest>() == 0)
        {
            return;
        }

        foreach (Entity entity in world.Entities<CarveRequest>().ToArray())
        {
            if (!world.TryGet(entity, out CarveRequest request))
            {
                continue;
            }

            Apply(world, entity, request);

            world.Remove<CarveRequest>(entity);
        }
    }

    private static SdfEdit EditFor(CarveRequest request) => (Tool)request.Tool switch
    {
        Tool.Chisel => SdfEdit.Box(request.Position, request.Size),
        Tool.Saw => SdfEdit.Plane(request.Position, Quaternion.Identity),
        Tool.Drill => SdfEdit.Cylinder(
            request.Position, request.Size.X, request.Size.Y
        ),
        _ => SdfEdit.Sphere(request.Position, request.Size.X)
    };

    private static void Apply(World world, Entity entity, CarveRequest request)
    {
        SdfEdit edit = EditFor(request).Subtracting();

        if (world.TryGet(entity, out Host host) && Blocked(world, entity, edit, host))
        {
            World.Log($"{(Tool)request.Tool} will not cut that material");

            return;
        }

        if (!world.Deform(entity, edit, out DeformResult cut))
        {
            return;
        }

        if (cut.VolumeRemoved <= 0.0f)
        {
            return;
        }

        s_spent += cut.VolumeRemoved;

        World.Log(
            $"{(Tool)request.Tool} removed {cut.VolumeRemoved:F4} m3 from {entity} "
            + $"({s_spent:F4} spent)"
        );
    }

    private static bool Blocked(World world, Entity entity, SdfEdit edit, Host host)
    {
        Span<uint> materials = stackalloc uint[8];

        int found = world.MaterialsUnder(entity, edit, materials);

        for (int index = 0; index < found && index < materials.Length; ++index)
        {
            if (materials[index] == host.Uncuttable)
            {
                return true;
            }
        }

        return false;
    }
}
