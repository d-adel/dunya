#include "instantiate.ih"

namespace dunya::objectmodel {

void instantiateWorld(const World& source, World& destination) {
  const entt::registry& registry = source.registry();

  // Through the mutation surface rather than a snapshot: the primitive arena
  // lives outside the registry, so a component copy would leave ranges pointing
  // into an empty pool.
  for (const Entity entity : source.fields()) {
    if (!destination.createFieldAt(
          entity,
          registry.get<Pose>(entity),
          registry.get<SdfGrid>(entity)
        )) {
      throw std::runtime_error("Cannot instantiate a field at its authored id");
    }

    // Named explicitly like everything else here: the list is what keeps
    // BakedVolume out, and it silently drops whatever nobody adds to it.
    if (registry.all_of<StaticBody>(entity)) {
      destination.addStaticBody(entity);
    }

    if (const auto* scale = registry.try_get<MassScale>(entity)) {
      destination.emplaceOrReplace<MassScale>(entity, *scale);
    }

    if (registry.all_of<Deformable>(entity)) {
      destination.emplaceOrReplace<Deformable>(entity, Deformable{});
    }

    for (const dunya::field::Primitive& primitive : source.primitives(entity)) {
      if (!destination.addPrimitive(entity, primitive)) {
        throw std::runtime_error("Cannot instantiate an object's primitives");
      }
    }

    // After the primitives, not before: adding one marks the field stale, and
    // the point of handing it over is that it is not.
    //
    // Shared rather than copied. The runtime reads the authored lattice until
    // it writes one, and a write goes through patchSampledField, which takes a
    // private copy first - so Play costs a handle per object rather than
    // 1.2 MB of it.
    if (const auto* held = registry.try_get<SharedField>(entity)) {
      destination.adoptSampledField(entity, *held);
    }

    // After the field, because setting one clears this: the copy carries
    // whatever the source lattice had been through, so the fact that it is no
    // longer the bake of its primitives has to travel with it.
    if (registry.all_of<Deformed>(entity)) {
      destination.emplaceOrReplace<Deformed>(entity, Deformed{});
    }
  }

  for (const Entity entity : source.meshes()) {
    if (!destination.createMeshAt(
          entity,
          registry.get<Pose>(entity),
          registry.get<Mesh>(entity),
          registry.get<Material>(entity)
        )) {
      throw std::runtime_error("Cannot instantiate a mesh at its authored id");
    }
  }
}

}  // namespace dunya::objectmodel
