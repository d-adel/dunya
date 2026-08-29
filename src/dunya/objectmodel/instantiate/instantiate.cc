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

    for (const dunya::field::Primitive& primitive : source.primitives(entity)) {
      if (!destination.addPrimitive(entity, primitive)) {
        throw std::runtime_error("Cannot instantiate an object's primitives");
      }
    }

    // After the primitives, not before: adding one marks the field stale, and
    // the point of copying it is that it is not. Copied rather than left to be
    // rebaked because a bake costs a second and a copy costs milliseconds.
    if (registry.all_of<dunya::field::SampledField>(entity)) {
      destination.setSampledField(
        entity,
        registry.get<dunya::field::SampledField>(entity)
      );
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
