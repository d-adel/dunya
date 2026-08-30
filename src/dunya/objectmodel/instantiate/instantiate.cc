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

  const auto reproduce = [&](Entity entity) {
    if (!destination.createAuthoredAt(entity)) {
      throw std::runtime_error(
        "Cannot instantiate an entity at its authored id"
      );
    }

    AuthoredComponents::each([&]<typename T>() {
      carry<T>(registry, destination, entity);
    });

    if (const auto* held = registry.try_get<SharedField>(entity)) {
      destination.adoptSampledField(entity, *held);
    }

    if (registry.all_of<Deformed>(entity)) {
      destination.emplaceOrReplace<Deformed>(entity, Deformed{});
    }
  };

  for (const Entity entity : source.fields()) {
    reproduce(entity);

    for (const dunya::field::Primitive& primitive : source.primitives(entity)) {
      if (!destination.addPrimitive(entity, primitive)) {
        throw std::runtime_error("Cannot instantiate an object's primitives");
      }
    }
  }

  for (const Entity entity : source.meshes()) {
    if (registry.all_of<SdfGrid>(entity)) {
      continue;
    }

    reproduce(entity);
  }
}

}
