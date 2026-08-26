#pragma once

#include "objectmodel/drawitem/drawitem.h"
#include "objectmodel/fieldobject/fieldobject.h"
#include "objectmodel/objectregistry/objectregistry.h"

#include <span>
#include <vector>

// The runtime state a frame reads and an edit changes: what exists and where
// it is. It carries no history, because undoing is something the editor does
// to a world rather than something a world knows about itself.
class World {
public:
  World() = default;

  World(const World&) = delete;
  World& operator=(const World&) = delete;
  World(World&&) = delete;
  World& operator=(World&&) = delete;

  const ObjectRegistry& registry() const noexcept;
  ObjectRegistry& registry() noexcept;

  std::span<const DrawItem> drawItems() const noexcept;
  void addDrawItem(const DrawItem& drawItem);

  ObjectId addFieldObject(const FieldObject& fieldObject);

  void setVolumeIndex(ObjectId objectId, uint32_t volumeIndex);
  void setDirty(ObjectId objectId, bool value);

private:
  ObjectRegistry m_objectRegistry;
  std::vector<DrawItem> m_drawItems;
};
