#include "worldfile.ih"

namespace dunya::serialize {

namespace {

constexpr uint32_t MATERIAL_LANE = 1u;

struct WorldOptions : glz::opts {
  bool new_lines_in_arrays = false;
  uint8_t indentation_width = 2;
};

bool carriesAnything(const StoredEntity& kept) {
  if (!kept.primitives.empty()) {
    return true;
  }

  bool any = false;

  std::apply(
    [&](auto... bound) {
      ((any = any || (kept.*(bound.slot)).has_value()), ...);
    },
    PORTABLE_COMPONENTS
  );

  return any;
}

template<dunya::objectmodel::Authored T>
std::optional<StoredType<T>> componentOf(
  const entt::registry& registry,
  dunya::objectmodel::Entity entity,
  const dunya::core::AssetDatabase& assets
) {
  if constexpr (dunya::objectmodel::AssetBacked<T>) {
    const T* value = registry.try_get<T>(entity);

    return value == nullptr
             ? std::nullopt
             : std::optional<dunya::core::AssetId>(assets.id<T>(value->index));
  } else if constexpr (std::is_empty_v<T>) {
    return registry.all_of<T>(entity) ? std::optional<T>(T{}) : std::nullopt;
  } else {
    const T* value = registry.try_get<T>(entity);

    return value == nullptr ? std::nullopt : std::optional<T>(*value);
  }
}

template<dunya::objectmodel::Authored T>
bool putBack(
  dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity,
  const std::optional<StoredType<T>>& stored,
  const dunya::core::AssetDatabase& assets
) {
  if (!stored.has_value()) {
    return true;
  }

  if constexpr (dunya::objectmodel::AssetBacked<T>) {
    const uint32_t index = assets.index<T>(*stored);

    if (index == dunya::core::UNBOUND_ASSET) {
      return false;
    }

    world.emplaceAuthored<T>(entity, T{index});
  } else {
    world.emplaceAuthored<T>(entity, *stored);
  }

  return true;
}

}

StoredWorld captureWorld(
  const dunya::objectmodel::World& world,
  const dunya::core::AssetDatabase& assets
) {
  const entt::registry& registry = world.registry();

  StoredWorld stored{};

  const auto keep = [&](dunya::objectmodel::Entity entity) {
    StoredEntity kept{};

    std::apply(
      [&](auto... bound) {
        ((kept.*(bound.slot) = componentOf<typename decltype(bound)::Component>(
            registry,
            entity,
            assets
          )),
         ...);
      },
      PORTABLE_COMPONENTS
    );

    return kept;
  };

  std::vector<dunya::objectmodel::Entity> order;

  for (const dunya::objectmodel::Entity entity :
       registry.view<entt::entity>()) {
    order.push_back(entity);
  }

  std::reverse(order.begin(), order.end());

  for (const dunya::objectmodel::Entity entity : order) {
    StoredEntity kept = keep(entity);

    if (registry.all_of<dunya::objectmodel::SdfGrid>(entity)) {
      for (const dunya::field::Primitive& primitive :
           world.primitives(entity)) {
        StoredPrimitive held{};

        held.primitive = primitive;

        held.material = assets.id<dunya::objectmodel::Material>(
          primitive.shapeConfig[MATERIAL_LANE]
        );

        held.primitive.shapeConfig[MATERIAL_LANE] = 0u;

        kept.primitives.push_back(held);
      }
    }

    if (!carriesAnything(kept)) {
      continue;
    }

    stored.entities.push_back(std::move(kept));
  }

  return stored;
}

bool restoreWorld(
  const StoredWorld& stored,
  dunya::objectmodel::World& world,
  const dunya::core::AssetDatabase& assets
) {
  if (stored.version > WORLD_VERSION) {
    return false;
  }

  for (const StoredEntity& kept : stored.entities) {
    const dunya::objectmodel::Entity entity = world.createAuthored();

    bool placed = true;

    std::apply(
      [&](auto... bound) {
        ((placed = putBack<typename decltype(bound)::Component>(
                     world,
                     entity,
                     kept.*(bound.slot),
                     assets
                   )
                   && placed),
         ...);
      },
      PORTABLE_COMPONENTS
    );

    if (!placed) {
      return false;
    }

    for (const StoredPrimitive& held : kept.primitives) {
      dunya::field::Primitive primitive = held.primitive;

      const uint32_t index =
        assets.index<dunya::objectmodel::Material>(held.material);

      if (index == dunya::core::UNBOUND_ASSET) {
        return false;
      }

      primitive.shapeConfig[MATERIAL_LANE] = index;

      if (!world.addPrimitive(entity, primitive)) {
        return false;
      }
    }
  }

  return true;
}

std::string writeWorld(const StoredWorld& stored) {
  std::string text;

  if (glz::write<WorldOptions{{.prettify = true}}>(stored, text)) {
    return std::string();
  }

  return text;
}

bool readWorld(std::string_view text, StoredWorld& stored) {
  return !glz::read<glz::opts{.error_on_unknown_keys = false}>(stored, text);
}

}
