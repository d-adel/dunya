#pragma once

#include <dunya/objectmodel/drawitem/drawitem.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>

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
  Entity addFieldObject(const FieldObject& fieldObject);

  bool addFieldObjectAt(Entity hint, const FieldObject& fieldObject);

  bool removeFieldObject(Entity entity);

  // Dense list of entities carrying FieldObject.
  std::span<const Entity> fieldObjects() const noexcept;

  // Primitive transactions.
  bool addPrimitive(Entity entity, const dunya::field::Primitive& primitive);

  bool insertPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  bool setPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  bool removePrimitive(Entity entity, uint32_t index);

  bool clearPrimitives(Entity entity);

  std::span<const dunya::field::Primitive> primitives(Entity entity) const;

  uint32_t primitiveCount(Entity entity) const;

  std::span<const dunya::field::Primitive> pool() const noexcept;

  // Component mutations that C still keeps on FieldObject.
  void setPose(
    Entity entity,
    const glm::vec3& position,
    const glm::quat& rotation
  );

  void setVolumeIndex(Entity entity, uint32_t volumeIndex);
  void setDirty(Entity entity, bool value);

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
