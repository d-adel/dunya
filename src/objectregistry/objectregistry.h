#pragma once

#include "config/config.h"
#include "field/field.h"
#include "fieldobject/fieldobject.h"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <span>
#include <vector>

class ObjectRegistry {
public:
  ObjectRegistry();

  ObjectRegistry(const ObjectRegistry&) = delete;
  ObjectRegistry& operator=(const ObjectRegistry&) = delete;
  ObjectRegistry(ObjectRegistry&&) = delete;
  ObjectRegistry& operator=(ObjectRegistry&&) = delete;

  ObjectId addFieldObject(const FieldObject& fieldObject);

  bool removeFieldObject(ObjectId objectId);

  bool addPrimitive(
    ObjectId objectId,
    const dunya::field::Primitive& primitive
  );

  bool removePrimitive(ObjectId objectId, uint32_t primitiveIndex);

  void clearPrimitives(ObjectId objectId);

  FieldObject& getFieldObject(ObjectId objectId);
  const FieldObject& getFieldObject(ObjectId objectId) const;

  std::span<dunya::field::Primitive> getPrimitives(ObjectId objectId);

  std::span<const dunya::field::Primitive> getPrimitives(
    ObjectId objectId
  ) const;

  std::span<const ObjectId> fieldObjectIds() const noexcept;

  bool contains(ObjectId objectId) const noexcept;

  uint32_t fieldObjectCount() const noexcept;
  uint32_t primitiveCount(ObjectId objectId) const;

  std::span<const dunya::field::Primitive> primitivePool() const noexcept;

  uint32_t primitiveOffset(ObjectId objectId) const;

private:
  struct FieldObjectSlot {
    std::optional<FieldObject> object;

    uint32_t primitiveOffset = 0;
    uint32_t primitiveCount = 0;
    uint32_t primitiveCapacity = 0;

    // Position in m_activeFieldObjects.
    // Lets removal use swap-and-pop in O(1).
    uint32_t activeIndex = 0;
  };

  struct PrimitiveRange {
    uint32_t offset;
    uint32_t capacity;
  };

  ObjectId allocateObjectId();

  std::optional<uint32_t> allocatePrimitiveRange(uint32_t capacity);

  void releasePrimitiveRange(uint32_t offset, uint32_t capacity);

  bool growPrimitiveRange(FieldObjectSlot& slot, uint32_t requiredCapacity);

  static std::optional<uint32_t> nextPrimitiveCapacity(
    uint32_t currentCapacity,
    uint32_t requiredCapacity
  );

  std::vector<FieldObjectSlot> m_fieldObjects;

  // Dense list of live IDs. This is what FrameContext can span.
  std::vector<ObjectId> m_activeFieldObjects;

  // Lowest recycled ObjectId comes out first.
  std::priority_queue<ObjectId, std::vector<ObjectId>, std::greater<ObjectId>>
    m_freeObjectIds;

  // First ObjectId that has never been used.
  ObjectId m_nextUnusedObjectId = 0;

  // Primitive arena.
  std::vector<dunya::field::Primitive> m_primitives;

  // offset -> capacity
  //
  // Ordered by offset so adjacent free ranges are easy to coalesce.
  std::map<uint32_t, uint32_t> m_freePrimitiveRanges;
};
