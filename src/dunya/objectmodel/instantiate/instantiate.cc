#include "instantiate.ih"

namespace dunya::objectmodel {

void instantiateWorld(const World& source, World& destination) {
  const entt::registry& registry = source.registry();

  for (const Entity entity : source.fields()) {
    if (!destination.createFieldAt(
          entity,
          registry.get<Pose>(entity),
          registry.get<SdfGrid>(entity)
        )) {
      throw std::runtime_error("Cannot instantiate a field at its authored id");
    }

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

    if (const auto* held = registry.try_get<SharedField>(entity)) {
      destination.adoptSampledField(entity, *held);
    }

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

}
