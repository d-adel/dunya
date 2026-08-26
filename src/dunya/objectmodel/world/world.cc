#include "world.ih"

namespace dunya::objectmodel {

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

dunya::core::ObjectId World::addFieldObject(const FieldObject& fieldObject) {
  return m_objectRegistry.addFieldObject(fieldObject);
}

void World::setVolumeIndex(
  dunya::core::ObjectId objectId,
  uint32_t volumeIndex
) {
  m_objectRegistry.getFieldObject(objectId).volumeIndex = volumeIndex;
}

void World::setDirty(dunya::core::ObjectId objectId, bool value) {
  m_objectRegistry.getFieldObject(objectId).dirty = value;
}

}  // namespace dunya::objectmodel
