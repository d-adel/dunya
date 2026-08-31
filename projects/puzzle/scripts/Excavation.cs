using Dunya.Engine;

namespace Puzzle;

public enum Turn : uint
{
    Planning = 0,
    Committing = 1,
    Settling = 2,
    Finished = 3
}

public enum Verdict : uint
{
    Undecided = 0,
    Freed = 1,
    OutOfAllowance = 2,
    PayloadBroken = 3
}

[Component]
public struct Excavation
{
    public float Allowance;
    public float Spent;
    public uint Turn;
    public uint Verdict;
}

[Component]
public struct Host
{
    public uint Uncuttable;
}

[Component]
public struct Payload
{
    public float Radius;
    public float Freed;
}

public static class Site
{
    public const uint Armour = 2u;

    public const float Allowance = 60.0f;

    public static bool Bounds(
        World world, out Vector3 low, out Vector3 high
    )
    {
        low = default;
        high = default;

        bool any = false;

        foreach (Entity chunk in world.Entities<Host>().ToArray())
        {
            if (!world.TryGetBounds(chunk, out Vector3 near, out Vector3 far))
            {
                continue;
            }

            if (!any)
            {
                low = near;
                high = far;
                any = true;

                continue;
            }

            low = new Vector3(
                Math.Min(low.X, near.X),
                Math.Min(low.Y, near.Y),
                Math.Min(low.Z, near.Z)
            );

            high = new Vector3(
                Math.Max(high.X, far.X),
                Math.Max(high.Y, far.Y),
                Math.Max(high.Z, far.Z)
            );
        }

        return any;
    }

    public static bool Overlaps(
        Vector3 lowA, Vector3 highA, Vector3 lowB, Vector3 highB
    )
        => lowA.X <= highB.X && highA.X >= lowB.X
           && lowA.Y <= highB.Y && highA.Y >= lowB.Y
           && lowA.Z <= highB.Z && highA.Z >= lowB.Z;
}

public sealed class RoleSystem : ScriptSystem
{
    public override int Order => Phases.ReadIntent;

    public override string Name => "roles";

    private bool m_assigned;

    public override void Run(World world)
    {
        if (m_assigned || world.Count<Excavation>() > 0)
        {
            m_assigned = true;

            return;
        }

        Entity[] live = world.All();

        if (live.Length == 0)
        {
            return;
        }

        var chunks = new List<Entity>();

        Entity payload = default;
        bool foundPayload = false;

        foreach (Entity entity in live)
        {
            bool anchored = world.Has(entity, "StaticBody");
            bool carvable = world.Has(entity, "Deformable");

            if (anchored && carvable)
            {
                chunks.Add(entity);
            }
            else if (!anchored && carvable && !foundPayload)
            {
                payload = entity;
                foundPayload = true;
            }
        }

        if (chunks.Count == 0 || !foundPayload)
        {
            return;
        }

        if (!world.TryGetPose(payload, out Pose seat))
        {
            return;
        }

        float radius = -world.SampleSdf(payload, seat.Position);

        if (radius <= 0.0f)
        {
            return;
        }

        foreach (Entity chunk in chunks)
        {
            world.Set(chunk, new Host { Uncuttable = Site.Armour });
        }

        world.Set(payload, new Payload { Radius = radius, Freed = 0.0f });

        world.Set(chunks[0], new Excavation
        {
            Allowance = Site.Allowance,
            Spent = 0.0f,
            Turn = (uint)Turn.Planning,
            Verdict = (uint)Verdict.Undecided
        });

        m_assigned = true;

        World.Log(
            $"site opened: {chunks.Count} host chunks, payload {payload} "
            + $"(r {radius:F3})"
        );
    }
}

public sealed class AllowanceSystem : ScriptSystem
{
    public override int Order => Phases.Excavate + 1;

    public override string Name => "allowance";

    public override void Run(World world)
    {
        foreach (Entity entity in world.Entities<Excavation>().ToArray())
        {
            ref Excavation excavation = ref world.Get<Excavation>(entity);

            if (excavation.Verdict != (uint)Verdict.Undecided)
            {
                continue;
            }

            excavation.Spent = CarveSystem.Spent;

            if (excavation.Spent > excavation.Allowance)
            {
                excavation.Verdict = (uint)Verdict.OutOfAllowance;
                excavation.Turn = (uint)Turn.Finished;

                World.Log(
                    $"allowance spent: {excavation.Spent:F3} of {excavation.Allowance:F3}"
                );
            }
        }
    }
}

public sealed class VerdictSystem : ScriptSystem
{
    public override int Order => Phases.Judge;

    public override string Name => "verdict";

    private static float Escape(Vector3 at, Vector3 low, Vector3 high)
    {
        float x = Math.Max(low.X - at.X, at.X - high.X);
        float y = Math.Max(low.Y - at.Y, at.Y - high.Y);
        float z = Math.Max(low.Z - at.Z, at.Z - high.Z);

        return Math.Max(x, Math.Max(y, z));
    }

    public override void Run(World world)
    {
        if (!Site.Bounds(world, out Vector3 low, out Vector3 high))
        {
            return;
        }

        foreach (Entity entity in world.Entities<Payload>().ToArray())
        {
            ref Payload payload = ref world.Get<Payload>(entity);

            if (!world.TryGetPose(entity, out Pose pose))
            {
                continue;
            }

            float outside = Escape(pose.Position, low, high);

            payload.Freed = outside;

            if (outside <= 0.0f)
            {
                continue;
            }

            foreach (Entity carrier in world.Entities<Excavation>().ToArray())
            {
                ref Excavation excavation = ref world.Get<Excavation>(carrier);

                if (excavation.Verdict != (uint)Verdict.Undecided)
                {
                    continue;
                }

                excavation.Verdict = (uint)Verdict.Freed;
                excavation.Turn = (uint)Turn.Finished;

                World.Log($"payload freed, {outside:F3} m clear of the host");
            }
        }
    }
}
