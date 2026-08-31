using System.Runtime.InteropServices;
using System.Text;

namespace Dunya.Engine;

internal static unsafe class Native
{
    public const uint InvalidComponent = uint.MaxValue;

    private static ApiTable s_api;
    private static bool s_ready;

    public static string? Refusal { get; private set; }

    public static bool Bind(ApiTable* api)
    {
        Refusal = null;

        if (api == null)
        {
            Refusal = "the engine passed no api table";

            return false;
        }

        if (api->Version != ApiTable.ExpectedVersion)
        {
            Refusal =
                $"api version {api->Version}, expected {ApiTable.ExpectedVersion}";

            return false;
        }

        if (api->Size != (uint)sizeof(ApiTable))
        {
            Refusal = $"api table is {api->Size} bytes, expected {sizeof(ApiTable)}";

            return false;
        }

        s_api = *api;
        s_ready = true;

        return true;
    }

    public static bool Ready => s_ready;

    private static byte* Utf8(string text, out IntPtr owned)
    {
        owned = Marshal.StringToCoTaskMemUTF8(text);

        return (byte*)owned;
    }

    public static uint DeclareComponent(
        void* world,
        string name,
        uint size,
        (string Name, FieldKind Kind, uint Offset)[] fields
    )
    {
        byte* utf8 = Utf8(name, out IntPtr owned);

        var descriptors = new FieldDescriptor[fields.Length];
        var fieldNames = new IntPtr[fields.Length];

        try
        {
            for (int index = 0; index < fields.Length; ++index)
            {
                fieldNames[index] = Marshal.StringToCoTaskMemUTF8(fields[index].Name);

                descriptors[index] = new FieldDescriptor
                {
                    Name = (byte*)fieldNames[index],
                    Kind = (uint)fields[index].Kind,
                    Offset = fields[index].Offset
                };
            }

            fixed (FieldDescriptor* first = descriptors)
            {
                return s_api.DeclareComponent(
                    world, utf8, size, first, (uint)descriptors.Length
                );
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(owned);

            foreach (IntPtr held in fieldNames)
            {
                if (held != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(held);
                }
            }
        }
    }

    public static uint FindComponent(void* world, string name)
    {
        byte* utf8 = Utf8(name, out IntPtr owned);

        try
        {
            return s_api.FindComponent(world, utf8);
        }
        finally
        {
            Marshal.FreeCoTaskMem(owned);
        }
    }

    public static bool SetComponent<T>(void* world, uint type, uint entity, in T value)
        where T : unmanaged
    {
        fixed (T* at = &value)
        {
            return s_api.SetComponent(world, type, entity, at) != 0;
        }
    }

    public static T* GetComponent<T>(void* world, uint type, uint entity)
        where T : unmanaged
    {
        return (T*)s_api.GetComponent(world, type, entity);
    }

    public static bool RemoveComponent(void* world, uint type, uint entity)
    {
        return s_api.RemoveComponent(world, type, entity) != 0;
    }

    public static uint ComponentCount(void* world, uint type)
    {
        return s_api.ComponentCount(world, type);
    }

    public static ReadOnlySpan<uint> Entities(void* world, uint type)
    {
        uint count = s_api.ComponentCount(world, type);
        uint* first = s_api.ComponentEntities(world, type);

        return count == 0 || first == null
            ? ReadOnlySpan<uint>.Empty
            : new ReadOnlySpan<uint>(first, (int)count);
    }

    public static Span<T> Components<T>(void* world, uint type) where T : unmanaged
    {
        uint count = s_api.ComponentCount(world, type);
        void* first = s_api.ComponentData(world, type);

        return count == 0 || first == null
            ? Span<T>.Empty
            : new Span<T>(first, (int)count);
    }

    public static bool Deform(
        void* world,
        uint entity,
        ref SdfEditDescriptor edit,
        out SdfDeformSummary result
    )
    {
        SdfDeformSummary filled = default;

        fixed (SdfEditDescriptor* at = &edit)
        {
            int ok = s_api.DeformSdf(world, entity, at, &filled);

            result = filled;

            return ok != 0;
        }
    }

    public static uint MaterialsUnder(
        void* world,
        uint entity,
        ref SdfEditDescriptor edit,
        Span<uint> materials
    )
    {
        fixed (SdfEditDescriptor* at = &edit)
        fixed (uint* first = materials)
        {
            return s_api.MaterialsUnderSdf(
                world, entity, at, first, (uint)materials.Length
            );
        }
    }

    public static float SampleSdf(void* world, uint entity, Vector3 point)
    {
        float* at = stackalloc float[3];

        at[0] = point.X;
        at[1] = point.Y;
        at[2] = point.Z;

        return s_api.SampleSdf(world, entity, at);
    }

    public static bool GetPose(void* world, uint entity, out Pose pose)
    {
        float* held = stackalloc float[7];

        int ok = s_api.GetPose(world, entity, held);

        pose = new Pose
        {
            Position = new Vector3(held[0], held[1], held[2]),
            Rotation = new Quaternion
            {
                X = held[3], Y = held[4], Z = held[5], W = held[6]
            }
        };

        return ok != 0;
    }

    public static bool SetPose(void* world, uint entity, in Pose pose)
    {
        float* held = stackalloc float[7];

        held[0] = pose.Position.X;
        held[1] = pose.Position.Y;
        held[2] = pose.Position.Z;
        held[3] = pose.Rotation.X;
        held[4] = pose.Rotation.Y;
        held[5] = pose.Rotation.Z;
        held[6] = pose.Rotation.W;

        return s_api.SetPose(world, entity, held) != 0;
    }

    public static uint Entities(void* world, Span<uint> buffer)
    {
        fixed (uint* first = buffer)
        {
            return s_api.Entities(world, first, (uint)buffer.Length);
        }
    }

    public static bool HasComponent(void* world, uint entity, string name)
    {
        byte* utf8 = Utf8(name, out IntPtr owned);

        try
        {
            return s_api.HasComponent(world, entity, utf8) != 0;
        }
        finally
        {
            Marshal.FreeCoTaskMem(owned);
        }
    }

    public static bool Bounds(
        void* world,
        uint entity,
        out Vector3 minimum,
        out Vector3 maximum
    )
    {
        float* low = stackalloc float[3];
        float* high = stackalloc float[3];

        int ok = s_api.Bounds(world, entity, low, high);

        minimum = new Vector3(low[0], low[1], low[2]);
        maximum = new Vector3(high[0], high[1], high[2]);

        return ok != 0;
    }

    public static void Log(string message)
    {
        byte* utf8 = Utf8(message, out IntPtr owned);

        try
        {
            s_api.Log(utf8);
        }
        finally
        {
            Marshal.FreeCoTaskMem(owned);
        }
    }

    public static bool AddSystem(
        void* schedule,
        int order,
        string name,
        delegate* unmanaged<void*, void*, float, uint, void> callback,
        void* user
    )
    {
        byte* utf8 = Utf8(name, out IntPtr owned);

        try
        {
            return s_api.AddSystem(schedule, order, utf8, callback, user) != 0;
        }
        finally
        {
            Marshal.FreeCoTaskMem(owned);
        }
    }
}
