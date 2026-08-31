#include "instantiate.ih"

namespace dunya::objectmodel {

namespace {

template<Authored T>
void carry(const entt::registry& source, World& destination, Entity entity) {
  if constexpr (std::is_empty_v<T>) {
    if (source.all_of<T>(entity)) {
      destination.emplaceAuthored<T>(entity, T{});
    }
  } else {
    if (const T* value = source.try_get<T>(entity)) {
      destination.emplaceAuthored<T>(entity, *value);
    }
  }
}

}

void instantiateWorld(const World& source, World& destination) {
  const entt::registry& registry = source.registry();

  for (ComponentType type = 0u; type < source.dynamic().types(); ++type) {
    const ComponentSpec* spec = source.dynamic().spec(type);

    if (
      spec == nullptr
      || destination.dynamic().declare(*spec) == INVALID_COMPONENT_TYPE
    ) {
      throw std::runtime_error("Cannot instantiate a declared component type");
    }
  }

  const auto reproduce = [&](Entity entity) {
    if (!destination.createAuthoredAt(entity)) {
      throw std::runtime_error(
        "Cannot instantiate an entity at its authored id"
      );
    }

    AuthoredComponents::each([&]<typename T>() {
      carry<T>(registry, destination, entity);
    });

    if (const auto* held = registry.try_get<SharedSdf>(entity)) {
      destination.adoptSampledSdf(entity, *held);
    }

    if (registry.all_of<Deformed>(entity)) {
      destination.emplaceOrReplace<Deformed>(entity, Deformed{});
    }

    for (ComponentType type = 0u; type < source.dynamic().types(); ++type) {
      const std::byte* value = source.dynamic().get(type, entity);

      if (value == nullptr) {
        continue;
      }

      const std::span<const std::byte> bytes(
        value,
        source.dynamic().spec(type)->size
      );

      if (!destination.dynamic().emplace(type, entity, bytes)) {
        throw std::runtime_error("Cannot instantiate a declared component");
      }
    }
  };

  for (const Entity entity : liveEntities(source)) {
    reproduce(entity);

    if (!registry.all_of<SdfGrid>(entity)) {
      continue;
    }

    for (const dunya::field::Primitive& primitive : source.primitives(entity)) {
      if (!destination.addPrimitive(entity, primitive)) {
        throw std::runtime_error("Cannot instantiate an object's primitives");
      }
    }
  }
}

}
