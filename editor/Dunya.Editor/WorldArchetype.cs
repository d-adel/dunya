using System;
using System.Collections.Generic;
using System.Linq;

namespace Dunya.Editor;

public sealed class WorldArchetype
{
    private static readonly (string Component, string Name)[] Subjects =
    {
        ("Lens", "Camera"),
        ("DirectionalLight", "Light"),
        ("Environment", "Environment"),
        ("Mesh", "Mesh"),
        ("SdfGrid", "Sdf")
    };

    private static readonly (string Component, string Word)[] Modifiers =
    {
        ("StaticBody", "Static"),
        ("RigidBody", "Rigid"),
        ("Deformable", "Deformable")
    };

    private WorldArchetype(string[] components, uint[] entities)
    {
        Components = components;
        Entities = entities;
        Alias = Name(components);
    }

    public string[] Components { get; }

    public uint[] Entities { get; }

    public string Alias { get; private set; }

    public int Count => Entities.Length;

    public string Detail => string.Join(", ", Components);

    public bool Contains(uint entity) => Array.IndexOf(Entities, entity) >= 0;

    public static IReadOnlyList<WorldArchetype> Group(
        IReadOnlyList<WorldEntity> contents
    )
    {
        WorldArchetype[] archetypes = contents
            .GroupBy(
                entity => string.Join(
                    "",
                    entity.Components.OrderBy(name => name, StringComparer.Ordinal)
                ),
                StringComparer.Ordinal
            )
            .Select(group => new WorldArchetype(
                group.First()
                    .Components
                    .OrderBy(name => name, StringComparer.Ordinal)
                    .ToArray(),
                group.Select(entity => entity.Id).OrderBy(id => id).ToArray()
            ))
            .OrderByDescending(archetype => archetype.Count)
            .ThenBy(archetype => archetype.Alias, StringComparer.Ordinal)
            .ToArray();

        Disambiguate(archetypes);

        return archetypes;
    }

    private static string Name(string[] components)
    {
        string subject = Subjects
            .Where(entry => components.Contains(entry.Component))
            .Select(entry => entry.Name)
            .FirstOrDefault() ?? "Entity";

        string[] words = Modifiers
            .Where(entry => components.Contains(entry.Component))
            .Select(entry => entry.Word)
            .ToArray();

        return words.Length == 0
            ? subject
            : string.Join(" ", words) + " " + subject;
    }

    private static void Disambiguate(WorldArchetype[] archetypes)
    {
        foreach (IGrouping<string, WorldArchetype> clash in archetypes
                     .GroupBy(archetype => archetype.Alias, StringComparer.Ordinal)
                     .Where(group => group.Count() > 1))
        {
            int at = 1;

            foreach (WorldArchetype archetype in clash)
            {
                archetype.Alias = $"{archetype.Alias} {at}";

                ++at;
            }
        }
    }
}
