#include "world.ih"

namespace dunya::objectmodel {

World::World() {
  m_primitiveStore.connect(m_registry);
}

const entt::registry& World::registry() const noexcept {
  return m_registry;
}

Entity World::addFieldObject(const FieldObject& fieldObject) {
  const Entity entity = m_registry.create();

  m_registry.emplace<FieldObject>(entity, fieldObject);

  return entity;
}

bool World::addFieldObjectAt(Entity hint, const FieldObject& fieldObject) {
  const Entity entity = m_registry.create(hint);

  // EnTT treats a hint as a request, not a requirement. If that exact
  // identity is unavailable it creates another entity instead.
  //
  // For undo/redo that is failure: identity is part of the operation.
  if (entity != hint) {
    m_registry.destroy(entity);
    return false;
  }

  m_registry.emplace<FieldObject>(entity, fieldObject);

  return true;
}

bool World::removeFieldObject(Entity entity) {
  if (!m_registry.valid(entity) || !m_registry.all_of<FieldObject>(entity)) {
    return false;
  }

  // If the entity has an SdfPrimitiveRange, destroying it publishes
  // on_destroy<SdfPrimitiveRange> before removing the component. The
  // primitive store receives that signal and releases the arena range.
  m_registry.destroy(entity);

  return true;
}

std::span<const Entity> World::fieldObjects() const noexcept {
  const auto* storage = m_registry.storage<FieldObject>();

  if (storage == nullptr) {
    return {};
  }

  return {storage->data(), storage->size()};
}

bool World::addPrimitive(
  Entity entity,
  const dunya::field::Primitive& primitive
) {
  return m_primitiveStore.append(m_registry, entity, primitive);
}

bool World::insertPrimitive(
  Entity entity,
  uint32_t index,
  const dunya::field::Primitive& primitive
) {
  return m_primitiveStore.insert(m_registry, entity, index, primitive);
}

bool World::setPrimitive(
  Entity entity,
  uint32_t index,
  const dunya::field::Primitive& primitive
) {
  return m_primitiveStore.set(m_registry, entity, index, primitive);
}

bool World::removePrimitive(Entity entity, uint32_t index) {
  return m_primitiveStore.remove(m_registry, entity, index);
}

bool World::clearPrimitives(Entity entity) {
  return m_primitiveStore.clear(m_registry, entity);
}

std::span<const dunya::field::Primitive> World::primitives(
  Entity entity
) const {
  return m_primitiveStore.primitives(m_registry, entity);
}

uint32_t World::primitiveCount(Entity entity) const {
  return m_primitiveStore.count(m_registry, entity);
}

std::span<const dunya::field::Primitive> World::pool() const noexcept {
  return m_primitiveStore.pool();
}

void World::setPose(
  Entity entity,
  const glm::vec3& position,
  const glm::quat& rotation
) {
  FieldObject& object = m_registry.get<FieldObject>(entity);

  object.position = position;
  object.rotation = rotation;
}

void World::setVolumeIndex(Entity entity, uint32_t volumeIndex) {
  m_registry.get<FieldObject>(entity).volumeIndex = volumeIndex;
}

void World::setDirty(Entity entity, bool value) {
  m_registry.get<FieldObject>(entity).dirty = value;
}

std::span<const DrawItem> World::drawItems() const noexcept {
  return m_drawItems;
}

void World::addDrawItem(const DrawItem& drawItem) {
  m_drawItems.push_back(drawItem);
}

}  // namespace dunya::objectmodel
