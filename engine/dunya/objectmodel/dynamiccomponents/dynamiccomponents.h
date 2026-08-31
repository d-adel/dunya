#pragma once

#include <dunya/objectmodel/entity/entity.h>

#include <entt/entity/sparse_set.hpp>

#include <cstddef>
#include <array>
#include <cstdint>
#include <string_view>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dunya::objectmodel {

enum class FieldKind : uint8_t {
  Float,
  Int,
  UInt,
  Bool,
  Vec2,
  Vec3,
  Vec4,
  Quat,
  Count
};

struct FieldKindInfo {
  FieldKind kind{};
  std::string_view name;
  uint32_t lanes = 1u;
  uint32_t laneBytes = 4u;
};

inline constexpr std::
  array<FieldKindInfo, static_cast<size_t>(FieldKind::Count)>
    FIELD_KINDS{
      {{FieldKind::Float, "float", 1u, 4u},
       {FieldKind::Int, "int", 1u, 4u},
       {FieldKind::UInt, "uint", 1u, 4u},
       {FieldKind::Bool, "bool", 1u, 1u},
       {FieldKind::Vec2, "vec2", 2u, 4u},
       {FieldKind::Vec3, "vec3", 3u, 4u},
       {FieldKind::Vec4, "vec4", 4u, 4u},
       {FieldKind::Quat, "quat", 4u, 4u}}
    };

[[nodiscard]] constexpr bool fieldKindsAreOrdered() {
  for (size_t index = 0; index < FIELD_KINDS.size(); ++index) {
    if (static_cast<size_t>(FIELD_KINDS[index].kind) != index) {
      return false;
    }
  }

  return true;
}

static_assert(
  fieldKindsAreOrdered(),
  "FIELD_KINDS must be indexed by its own enumerator"
);

[[nodiscard]] constexpr const FieldKindInfo& fieldKindInfo(
  FieldKind kind
) noexcept {
  return FIELD_KINDS[static_cast<size_t>(kind)];
}

[[nodiscard]] constexpr bool isFieldKind(uint32_t stored) noexcept {
  return stored < static_cast<uint32_t>(FieldKind::Count);
}

[[nodiscard]] uint32_t fieldSize(FieldKind kind) noexcept;

[[nodiscard]] uint32_t fieldLanes(FieldKind kind) noexcept;

[[nodiscard]] std::string_view fieldKindName(FieldKind kind) noexcept;

struct FieldSpec {
  std::string name;
  FieldKind kind = FieldKind::Float;
  uint32_t offset = 0u;
};

struct ComponentSpec {
  std::string name;
  uint32_t size = 0u;
  std::vector<FieldSpec> fields;
};

using ComponentType = uint32_t;

inline constexpr ComponentType INVALID_COMPONENT_TYPE = UINT32_MAX;

class DynamicComponents {
public:
  [[nodiscard]] ComponentType declare(ComponentSpec spec);

  [[nodiscard]] ComponentType find(std::string_view name) const noexcept;

  [[nodiscard]] const ComponentSpec* spec(ComponentType type) const noexcept;

  [[nodiscard]] size_t types() const noexcept;

  [[nodiscard]] bool emplace(
    ComponentType type,
    Entity entity,
    std::span<const std::byte> value
  );

  [[nodiscard]] bool remove(ComponentType type, Entity entity);

  [[nodiscard]] bool contains(ComponentType type, Entity entity) const noexcept;

  [[nodiscard]] std::byte* get(ComponentType type, Entity entity) noexcept;

  [[nodiscard]] const std::byte* get(
    ComponentType type,
    Entity entity
  ) const noexcept;

  [[nodiscard]] std::span<const Entity> entities(
    ComponentType type
  ) const noexcept;

  [[nodiscard]] std::span<std::byte> data(ComponentType type) noexcept;

  [[nodiscard]] std::span<const std::byte> data(
    ComponentType type
  ) const noexcept;

  [[nodiscard]] size_t count(ComponentType type) const noexcept;

  void clear() noexcept;

  void clearEntities() noexcept;

  void clear(Entity entity);

private:
  struct Store {
    ComponentSpec spec;
    entt::basic_sparse_set<Entity> set;
    std::vector<std::byte> bytes;
  };

  [[nodiscard]] Store* store(ComponentType type) noexcept;
  [[nodiscard]] const Store* store(ComponentType type) const noexcept;

  std::vector<Store> m_stores;
};

[[nodiscard]] bool sameLayout(const ComponentSpec& a, const ComponentSpec& b);

}
