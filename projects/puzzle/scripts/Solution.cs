using System.Text.Json;
using Dunya.Engine;

namespace Puzzle;

public sealed class PlannedCut
{
    public uint Tool { get; set; }
    public float X { get; set; }
    public float Y { get; set; }
    public float Z { get; set; }
    public float Sx { get; set; }
    public float Sy { get; set; }
    public float Sz { get; set; }
}

public sealed class Solution
{
    public List<PlannedCut> Cuts { get; set; } = new();
}

public sealed class SolutionSystem : ScriptSystem
{
    public override int Order => Phases.ReadIntent + 1;

    public override string Name => "solution";

    private Solution? m_solution;
    private bool m_read;
    private int m_next;
    private uint m_waitUntil = 90u;

    public override void Run(World world)
    {
        if (!m_read)
        {
            m_read = true;
            m_solution = Read();
        }

        if (m_solution == null || m_next >= m_solution.Cuts.Count)
        {
            return;
        }

        if (world.Count<Host>() == 0 || world.Frame < m_waitUntil)
        {
            return;
        }

        PlannedCut cut = m_solution.Cuts[m_next++];

        CarveSystem.Reach(
            (Tool)cut.Tool,
            new Vector3(cut.X, cut.Y, cut.Z),
            new Vector3(cut.Sx, cut.Sy, cut.Sz),
            out Vector3 low,
            out Vector3 high
        );

        int reached = 0;

        foreach (Entity chunk in world.Entities<Host>().ToArray())
        {
            if (!world.TryGetBounds(chunk, out Vector3 near, out Vector3 far)
                || !Site.Overlaps(low, high, near, far))
            {
                continue;
            }

            world.Set(chunk, new CarveRequest
            {
                Tool = cut.Tool,
                Position = new Vector3(cut.X, cut.Y, cut.Z),
                Size = new Vector3(cut.Sx, cut.Sy, cut.Sz)
            });

            ++reached;
        }

        m_waitUntil = world.Frame + 30u;

        World.Log(
            $"solution cut {m_next} of {m_solution.Cuts.Count} queued on {reached} chunk(s)"
        );
    }


    private static Solution? Read()
    {
        string[] arguments = Environment.GetCommandLineArgs();

        for (int index = 0; index + 1 < arguments.Length; ++index)
        {
            if (arguments[index] != "--solution")
            {
                continue;
            }

            string path = arguments[index + 1];

            if (!File.Exists(path))
            {
                World.Log("no solution at " + path);

                return null;
            }

            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            };

            Solution? read =
                JsonSerializer.Deserialize<Solution>(File.ReadAllText(path), options);

            World.Log($"solution read: {read?.Cuts.Count ?? 0} cuts");

            return read;
        }

        return null;
    }
}
