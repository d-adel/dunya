#pragma once

#include <dunya/field/field.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/rangestore/rangestore.h>

#include <entt/entity/fwd.hpp>

#include <cstdint>
#include <span>

namespace dunya::objectmodel {

// Where an entity's primitives sit in the one contiguous pool. The GPU reads
// offset and count; capacity is the arena's business and never leaves the CPU.
struct SdfPrimitiveRange {
  uint32_t offset = 0;
  uint32_t count = 0;
  uint32_t capacity = 0;
};

// The arena the registry does not own. Every mutation is a transaction: move
// the elements, update the range, re-fit the grid - which signals the bake
// queue.
class SdfPrimitiveStore {
public:
  SdfPrimitiveStore();

  SdfPrimitiveStore(const SdfPrimitiveStore&) = delete;
  SdfPrimitiveStore& operator=(const SdfPrimitiveStore&) = delete;
  SdfPrimitiveStore(SdfPrimitiveStore&&) = delete;
  SdfPrimitiveStore& operator=(SdfPrimitiveStore&&) = delete;

  // Releasing a range is a lifetime invariant rather than a transaction: it has
  // to hold however the component goes away, including registry::clear. The
  // listener captures this store, so the store must outlive the registry.
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

  // The whole arena, which is what the GPU is handed.
  std::span<const dunya::field::Primitive> pool() const noexcept;

private:
  void onDestroyRange(entt::registry& registry, Entity entity);

  void refresh(entt::registry& registry, Entity entity);

  RangeStore<dunya::field::Primitive> m_primitives;
};

}  // namespace dunya::objectmodel
