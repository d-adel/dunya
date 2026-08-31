#include "worldfile.ih"

namespace dunya::serialize {

namespace {

constexpr uint32_t MATERIAL_LANE = 1u;

struct WorldOptions : glz::opts {
  bool new_lines_in_arrays = false;
  uint8_t indentation_width = 2;
};

bool carriesAnything(const StoredEntity& kept) {
  if (!kept.primitives.empty() || !kept.dynamic.empty()) {
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

namespace {

void readLanes(
  const std::byte* at,
  dunya::objectmodel::FieldKind kind,
  std::vector<double>& into
) {
  const uint32_t lanes = dunya::objectmodel::fieldLanes(kind);

  for (uint32_t lane = 0u; lane < lanes; ++lane) {
    switch (kind) {
      case dunya::objectmodel::FieldKind::Int: {
        int32_t held = 0;
        std::memcpy(&held, at + lane * 4u, 4u);
        into.push_back(static_cast<double>(held));
        break;
      }
      case dunya::objectmodel::FieldKind::UInt: {
        uint32_t held = 0u;
        std::memcpy(&held, at + lane * 4u, 4u);
        into.push_back(static_cast<double>(held));
        break;
      }
      case dunya::objectmodel::FieldKind::Bool: {
        uint8_t held = 0u;
        std::memcpy(&held, at + lane, 1u);
        into.push_back(held == 0u ? 0.0 : 1.0);
        break;
      }
      default: {
        float held = 0.0f;
        std::memcpy(&held, at + lane * 4u, 4u);
        into.push_back(static_cast<double>(held));
        break;
      }
    }
  }
}

void writeLanes(
  std::byte* at,
  dunya::objectmodel::FieldKind kind,
  const std::vector<double>& from,
  size_t& cursor
) {
  const uint32_t lanes = dunya::objectmodel::fieldLanes(kind);

  for (uint32_t lane = 0u; lane < lanes && cursor < from.size(); ++lane) {
    const double held = from[cursor++];

    switch (kind) {
      case dunya::objectmodel::FieldKind::Int: {
        const int32_t value = static_cast<int32_t>(held);
        std::memcpy(at + lane * 4u, &value, 4u);
        break;
      }
      case dunya::objectmodel::FieldKind::UInt: {
        const uint32_t value = static_cast<uint32_t>(held);
        std::memcpy(at + lane * 4u, &value, 4u);
        break;
      }
      case dunya::objectmodel::FieldKind::Bool: {
        const uint8_t value = held == 0.0 ? 0u : 1u;
        std::memcpy(at + lane, &value, 1u);
        break;
      }
      default: {
        const float value = static_cast<float>(held);
        std::memcpy(at + lane * 4u, &value, 4u);
        break;
      }
    }
  }
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

    const dunya::objectmodel::DynamicComponents& dynamic = world.dynamic();

    for (dunya::objectmodel::ComponentType type = 0u; type < dynamic.types();
         ++type) {
      const std::byte* at = dynamic.get(type, entity);

      if (at == nullptr) {
        continue;
      }

      const dunya::objectmodel::ComponentSpec* spec = dynamic.spec(type);

      StoredDynamic held{};
      held.type = spec->name;

      for (const dunya::objectmodel::FieldSpec& field : spec->fields) {
        readLanes(at + field.offset, field.kind, held.values);
      }

      kept.dynamic.push_back(std::move(held));
    }

    if (!carriesAnything(kept)) {
      continue;
    }

    stored.entities.push_back(std::move(kept));
  }

  for (dunya::objectmodel::ComponentType type = 0u;
       type < world.dynamic().types();
       ++type) {
    const dunya::objectmodel::ComponentSpec* spec = world.dynamic().spec(type);

    StoredComponentType described{};
    described.name = spec->name;
    described.size = spec->size;

    for (const dunya::objectmodel::FieldSpec& field : spec->fields) {
      described.fields.push_back(
        StoredField{field.name, static_cast<uint32_t>(field.kind), field.offset}
      );
    }

    stored.componentTypes.push_back(std::move(described));
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

  for (const StoredComponentType& described : stored.componentTypes) {
    dunya::objectmodel::ComponentSpec spec{};
    spec.name = described.name;
    spec.size = described.size;

    for (const StoredField& field : described.fields) {
      spec.fields.push_back(
        dunya::objectmodel::FieldSpec{
          field.name,
          static_cast<dunya::objectmodel::FieldKind>(field.kind),
          field.offset
        }
      );
    }

    if (
      world.dynamic().declare(spec)
      == dunya::objectmodel::INVALID_COMPONENT_TYPE
    ) {
      return false;
    }
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

    for (const StoredDynamic& held : kept.dynamic) {
      const dunya::objectmodel::ComponentType type =
        world.dynamic().find(held.type);

      const dunya::objectmodel::ComponentSpec* spec =
        world.dynamic().spec(type);

      if (spec == nullptr) {
        return false;
      }

      std::vector<std::byte> bytes(spec->size, std::byte{0});

      size_t cursor = 0u;

      for (const dunya::objectmodel::FieldSpec& field : spec->fields) {
        writeLanes(
          bytes.data() + field.offset,
          field.kind,
          held.values,
          cursor
        );
      }

      if (!world.dynamic().emplace(type, entity, bytes)) {
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
