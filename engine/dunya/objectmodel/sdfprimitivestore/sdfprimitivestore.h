#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>

#include <dunya/field/field.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/rangestore/rangestore.h>

#include <entt/entity/fwd.hpp>

#include <cstdint>
#include <span>

namespace dunya::objectmodel {

struct SdfPrimitiveRange {
  uint32_t offset = 0;
  uint32_t count = 0;
  uint32_t capacity = 0;
};

class SdfPrimitiveStore {
public:
  SdfPrimitiveStore();

  SdfPrimitiveStore(const SdfPrimitiveStore&) = delete;
  SdfPrimitiveStore& operator=(const SdfPrimitiveStore&) = delete;
  SdfPrimitiveStore(SdfPrimitiveStore&&) = delete;
  SdfPrimitiveStore& operator=(SdfPrimitiveStore&&) = delete;

  void connect(entt::registry& registry);

  [[nodiscard]]
  bool insert(
    entt::registry& registry,
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool append(
    entt::registry& registry,
    Entity entity,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool set(
    entt::registry& registry,
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool remove(entt::registry& registry, Entity entity, uint32_t index);

  [[nodiscard]]
  bool clear(entt::registry& registry, Entity entity);

  std::span<const dunya::field::Primitive> primitives(
    const entt::registry& registry,
    Entity entity
  ) const;

  uint32_t count(const entt::registry& registry, Entity entity) const;

  std::span<const dunya::field::Primitive> pool() const noexcept;

private:
  void onDestroyRange(entt::registry& registry, Entity entity);

  void refresh(entt::registry& registry, Entity entity);

  RangeStore<dunya::field::Primitive> m_primitives;
};

template<>
inline constexpr bool transient<SdfPrimitiveRange> = true;

}
