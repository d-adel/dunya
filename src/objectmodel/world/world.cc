#include "world.ih"

const ObjectRegistry& World::registry() const noexcept {
  return m_objectRegistry;
}

ObjectRegistry& World::registry() noexcept {
  return m_objectRegistry;
}

std::span<const DrawItem> World::drawItems() const noexcept {
  return m_drawItems;
}

void World::addDrawItem(const DrawItem& drawItem) {
  m_drawItems.push_back(drawItem);
}

ObjectId World::addFieldObject(const FieldObject& fieldObject) {
  return m_objectRegistry.addFieldObject(fieldObject);
}

void World::setVolumeIndex(ObjectId objectId, uint32_t volumeIndex) {
  m_objectRegistry.getFieldObject(objectId).volumeIndex = volumeIndex;
}

void World::setDirty(ObjectId objectId, bool value) {
  m_objectRegistry.getFieldObject(objectId).dirty = value;
}
