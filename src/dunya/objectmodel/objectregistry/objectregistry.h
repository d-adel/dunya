#pragma once

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <span>
#include <vector>

namespace dunya::objectmodel {

class ObjectRegistry {
public:
  ObjectRegistry();

  ObjectRegistry(const ObjectRegistry&) = delete;
  ObjectRegistry& operator=(const ObjectRegistry&) = delete;
  ObjectRegistry(ObjectRegistry&&) = delete;
  ObjectRegistry& operator=(ObjectRegistry&&) = delete;

  dunya::core::ObjectId addFieldObject(const FieldObject& fieldObject);

  // create
  bool addFieldObjectAt(dunya::core::ObjectId id, const FieldObject& object);

  bool addPrimitive(
    dunya::core::ObjectId objectId,
    const dunya::field::Primitive& primitive
  );

  // retrieve

  const FieldObject& getFieldObject(dunya::core::ObjectId objectId) const;

  std::span<const dunya::field::Primitive> getPrimitives(
    dunya::core::ObjectId objectId
  ) const;

  std::span<const dunya::core::ObjectId> fieldObjectIds() const noexcept;

  std::span<const dunya::field::Primitive> primitivePool() const noexcept;

  FieldObject& getFieldObject(dunya::core::ObjectId objectId);

  std::span<dunya::field::Primitive> getPrimitives(
    dunya::core::ObjectId objectId
  );

  // update
  bool setPrimitive(
    dunya::core::ObjectId objectId,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  bool insertPrimitive(
    dunya::core::ObjectId objectId,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  bool removePrimitive(dunya::core::ObjectId objectId, uint32_t primitiveIndex);

  void clearPrimitives(dunya::core::ObjectId objectId);
  // delete
  bool removeFieldObject(dunya::core::ObjectId objectId);

  // helpers
  bool contains(dunya::core::ObjectId objectId) const noexcept;
  uint32_t fieldObjectCount() const noexcept;
  uint32_t primitiveCount(dunya::core::ObjectId objectId) const;
  uint32_t primitiveOffset(dunya::core::ObjectId objectId) const;

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

  dunya::core::ObjectId allocateObjectId();

  std::optional<uint32_t> allocatePrimitiveRange(uint32_t capacity);

  void releasePrimitiveRange(uint32_t offset, uint32_t capacity);

  bool growPrimitiveRange(FieldObjectSlot& slot, uint32_t requiredCapacity);

  static std::optional<uint32_t> nextPrimitiveCapacity(
    uint32_t currentCapacity,
    uint32_t requiredCapacity
  );

  std::vector<FieldObjectSlot> m_fieldObjects;

  // Dense list of live IDs. This is what FrameContext can span.
  std::vector<dunya::core::ObjectId> m_activeFieldObjects;

  // Lowest recycled ObjectId comes out first.
  std::priority_queue<
    dunya::core::ObjectId,
    std::vector<dunya::core::ObjectId>,
    std::greater<dunya::core::ObjectId>>
    m_freeObjectIds;

  // First ObjectId that has never been used.
  dunya::core::ObjectId m_nextUnusedObjectId = 0;

  // Primitive arena.
  std::vector<dunya::field::Primitive> m_primitives;

  // offset -> capacity
  //
  // Ordered by offset so adjacent free ranges are easy to coalesce.
  std::map<uint32_t, uint32_t> m_freePrimitiveRanges;
};

}  // namespace dunya::objectmodel
