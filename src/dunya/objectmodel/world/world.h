#pragma once

#include <dunya/objectmodel/drawitem/drawitem.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/fieldgrid/fieldgrid.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::objectmodel {

class World {
public:
  World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;
  World(World&&) = delete;
  World& operator=(World&&) = delete;

  // Read-only from outside World.
  const entt::registry& registry() const noexcept;

  // Field-object lifetime.
  Entity createField(const Pose& pose, const FieldGrid& grid);

  [[nodiscard]]
  bool createFieldAt(Entity hint, const Pose& pose, const FieldGrid& grid);

  [[nodiscard]]
  bool destroyField(Entity entity);

  // Dense list of entities carrying FieldGrid.
  std::span<const Entity> fields() const noexcept;

  // Primitive transactions.
  [[nodiscard]]
  bool addPrimitive(Entity entity, const dunya::field::Primitive& primitive);

  [[nodiscard]]
  bool insertPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool setPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool removePrimitive(Entity entity, uint32_t index);

  std::span<const dunya::field::Primitive> primitives(Entity entity) const;

  uint32_t primitiveCount(Entity entity) const;

  std::span<const dunya::field::Primitive> pool() const noexcept;

  // Component mutations that C still keeps on FieldGrid.
  void setPose(
    Entity entity,
    const glm::vec3& position,
    const glm::quat& rotation
  );

  void setBakedVolume(Entity entity, uint32_t index);

  // Change tracking, not a flag. The queue is an entt::reactive storage the
  // registry fills from its own signals, so a mutation path added later is
  // covered without anyone remembering to mark it - and a second consumer
  // (a physics re-fit, an acceleration structure) gets its own window off the
  // same signal instead of fighting over one bool.
  bool needsBake(Entity entity) const noexcept;
  void markBaked(Entity entity);

  // Existing non-field world data.
  std::span<const DrawItem> drawItems() const noexcept;
  void addDrawItem(const DrawItem& drawItem);

private:
  // MUST outlive m_registry because its on_destroy listener refers to this
  // store. Members are destroyed in reverse declaration order.
  SdfPrimitiveStore m_primitiveStore;

  entt::registry m_registry;

  std::vector<DrawItem> m_drawItems;
};

}  // namespace dunya::objectmodel
