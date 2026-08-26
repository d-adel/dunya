#include "objectregistry.ih"

namespace dunya::objectmodel {

using dunya::field::Primitive;

namespace {

constexpr uint32_t INITIAL_PRIMITIVE_CAPACITY = 4;

}

ObjectRegistry::ObjectRegistry()
    : m_fieldObjects(dunya::core::MAX_FIELD_OBJECTS) {
  m_activeFieldObjects.reserve(dunya::core::MAX_FIELD_OBJECTS);
}

dunya::core::ObjectId ObjectRegistry::allocateObjectId() {
  while (!m_freeObjectIds.empty()) {
    const dunya::core::ObjectId objectId = m_freeObjectIds.top();
    m_freeObjectIds.pop();

    if (!m_fieldObjects[objectId].object.has_value()) {
      return objectId;
    }
  }

  if (m_nextUnusedObjectId >= dunya::core::MAX_FIELD_OBJECTS) {
    return dunya::core::INVALID_OBJECT_ID;
  }

  return m_nextUnusedObjectId++;
}

dunya::core::ObjectId ObjectRegistry::addFieldObject(
  const FieldObject& fieldObject
) {
  const dunya::core::ObjectId objectId = allocateObjectId();

  if (objectId == dunya::core::INVALID_OBJECT_ID) {
    return dunya::core::INVALID_OBJECT_ID;
  }

  addFieldObjectAt(objectId, fieldObject);

  return objectId;
}

bool ObjectRegistry::addFieldObjectAt(
  dunya::core::ObjectId objectId,
  const FieldObject& fieldObject
) {
  if (
    objectId >= dunya::core::MAX_FIELD_OBJECTS
    || m_fieldObjects[objectId].object.has_value()
  ) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  slot.object = fieldObject;

  slot.primitiveOffset = 0;
  slot.primitiveCount = 0;
  slot.primitiveCapacity = 0;

  slot.activeIndex = static_cast<uint32_t>(m_activeFieldObjects.size());

  m_activeFieldObjects.push_back(objectId);

  std::span<dunya::field::Primitive> primitives = getPrimitives(objectId);

  refreshDerived(getFieldObject(objectId), primitives);

  return true;
}

bool ObjectRegistry::setPrimitive(
  dunya::core::ObjectId objectId,
  uint32_t primitiveIndex,
  const dunya::field::Primitive& primitive
) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (primitiveIndex >= slot.primitiveCount) {
    return false;
  }

  m_primitives[slot.primitiveOffset + primitiveIndex] = primitive;

  std::span<dunya::field::Primitive> primitives = getPrimitives(objectId);

  FieldObject& object = getFieldObject(objectId);

  refreshDerived(object, primitives);

  object.dirty = true;

  return true;
}

bool ObjectRegistry::insertPrimitive(
  dunya::core::ObjectId objectId,
  uint32_t primitiveIndex,
  const dunya::field::Primitive& primitive
) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (primitiveIndex > slot.primitiveCount) {
    return false;
  }

  if (slot.primitiveCount >= dunya::core::MAX_FIELD_PRIMITIVES) {
    return false;
  }

  if (slot.primitiveCount == slot.primitiveCapacity) {
    uint32_t newCapacity =
      slot.primitiveCapacity == 0 ? 1 : slot.primitiveCapacity * 2;

    newCapacity = std::min(
      newCapacity,
      static_cast<uint32_t>(dunya::core::MAX_FIELD_PRIMITIVES)
    );

    std::optional<const uint32_t> newOffset =
      allocatePrimitiveRange(newCapacity);

    if (!newOffset.has_value()) {
      return false;
    }

    if (newOffset.value() == dunya::core::INVALID_PRIMITIVE_OFFSET) {
      return false;
    }

    if (slot.primitiveCount > 0) {
      std::move(
        m_primitives.begin() + slot.primitiveOffset,
        m_primitives.begin() + slot.primitiveOffset + slot.primitiveCount,
        m_primitives.begin() + newOffset.value()
      );
    }

    if (slot.primitiveCapacity > 0) {
      releasePrimitiveRange(slot.primitiveOffset, slot.primitiveCapacity);
    }

    slot.primitiveOffset = newOffset.value();
    slot.primitiveCapacity = newCapacity;
  }

  auto begin = m_primitives.begin() + slot.primitiveOffset;

  std::move_backward(
    begin + primitiveIndex,
    begin + slot.primitiveCount,
    begin + slot.primitiveCount + 1
  );

  begin[primitiveIndex] = primitive;

  ++slot.primitiveCount;

  std::span<dunya::field::Primitive> primitives = getPrimitives(objectId);

  FieldObject& object = getFieldObject(objectId);

  refreshDerived(object, primitives);

  object.dirty = true;

  return true;
}

bool ObjectRegistry::removeFieldObject(dunya::core::ObjectId objectId) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (slot.primitiveCapacity > 0) {
    releasePrimitiveRange(slot.primitiveOffset, slot.primitiveCapacity);
  }

  const uint32_t removedIndex = slot.activeIndex;
  const dunya::core::ObjectId lastObjectId = m_activeFieldObjects.back();

  if (removedIndex != m_activeFieldObjects.size() - 1) {
    m_activeFieldObjects[removedIndex] = lastObjectId;
    m_fieldObjects[lastObjectId].activeIndex = removedIndex;
  }

  m_activeFieldObjects.pop_back();

  slot.object.reset();
  slot.primitiveOffset = 0;
  slot.primitiveCount = 0;
  slot.primitiveCapacity = 0;
  slot.activeIndex = 0;

  m_freeObjectIds.push(objectId);

  return true;
}

std::optional<uint32_t> ObjectRegistry::allocatePrimitiveRange(
  uint32_t capacity
) {
  if (capacity == 0) {
    return 0;
  }

  auto best = m_freePrimitiveRanges.end();

  for (auto it = m_freePrimitiveRanges.begin();
       it != m_freePrimitiveRanges.end();
       ++it) {
    if (it->second < capacity) {
      continue;
    }

    if (best == m_freePrimitiveRanges.end() || it->second < best->second) {
      best = it;
    }
  }

  if (best != m_freePrimitiveRanges.end()) {
    const uint32_t offset = best->first;
    const uint32_t available = best->second;

    m_freePrimitiveRanges.erase(best);

    if (available > capacity) {
      m_freePrimitiveRanges.emplace(offset + capacity, available - capacity);
    }

    return offset;
  }

  if (m_primitives.size() > std::numeric_limits<uint32_t>::max() - capacity) {
    throw std::overflow_error(
      "Primitive arena exceeded uint32_t address space"
    );
  }

  if (m_primitives.size() + capacity > dunya::core::MAX_PRIMITIVE_POOL) {
    return std::nullopt;
  }

  const uint32_t offset = static_cast<uint32_t>(m_primitives.size());

  m_primitives.resize(m_primitives.size() + capacity);

  return offset;
}

void ObjectRegistry::releasePrimitiveRange(uint32_t offset, uint32_t capacity) {
  if (capacity == 0) {
    return;
  }

  uint32_t mergedOffset = offset;
  uint32_t mergedCapacity = capacity;

  auto next = m_freePrimitiveRanges.lower_bound(offset);

  if (next != m_freePrimitiveRanges.begin()) {
    auto previous = std::prev(next);

    const uint32_t previousEnd = previous->first + previous->second;

    if (previousEnd == offset) {
      mergedOffset = previous->first;
      mergedCapacity += previous->second;

      m_freePrimitiveRanges.erase(previous);
    }
  }

  next = m_freePrimitiveRanges.lower_bound(mergedOffset);

  if (next != m_freePrimitiveRanges.end()) {
    const uint32_t mergedEnd = mergedOffset + mergedCapacity;

    if (mergedEnd == next->first) {
      mergedCapacity += next->second;

      m_freePrimitiveRanges.erase(next);
    }
  }

  if (mergedOffset + mergedCapacity == m_primitives.size()) {
    m_primitives.resize(mergedOffset);
    return;
  }

  m_freePrimitiveRanges.emplace(mergedOffset, mergedCapacity);
}

std::optional<uint32_t> ObjectRegistry::nextPrimitiveCapacity(
  uint32_t currentCapacity,
  uint32_t requiredCapacity
) {
  if (requiredCapacity > dunya::core::MAX_FIELD_PRIMITIVES) {
    return std::nullopt;
  }

  uint32_t capacity =
    currentCapacity == 0
      ? std::min(INITIAL_PRIMITIVE_CAPACITY, dunya::core::MAX_FIELD_PRIMITIVES)
      : currentCapacity;

  while (capacity < requiredCapacity) {
    if (capacity >= dunya::core::MAX_FIELD_PRIMITIVES) {
      return std::nullopt;
    }

    capacity = std::min(capacity * 2, dunya::core::MAX_FIELD_PRIMITIVES);
  }

  return capacity;
}

bool ObjectRegistry::growPrimitiveRange(
  FieldObjectSlot& slot,
  uint32_t requiredCapacity
) {
  const std::optional<uint32_t> newCapacity =
    nextPrimitiveCapacity(slot.primitiveCapacity, requiredCapacity);

  if (!newCapacity) {
    return false;
  }

  const std::optional<uint32_t> newOffset =
    allocatePrimitiveRange(*newCapacity);

  if (!newOffset) {
    return false;
  }

  for (uint32_t i = 0; i < slot.primitiveCount; ++i) {
    m_primitives[*newOffset + i] =
      std::move(m_primitives[slot.primitiveOffset + i]);
  }

  if (slot.primitiveCapacity > 0) {
    releasePrimitiveRange(slot.primitiveOffset, slot.primitiveCapacity);
  }

  slot.primitiveOffset = *newOffset;
  slot.primitiveCapacity = *newCapacity;

  return true;
}

bool ObjectRegistry::addPrimitive(
  dunya::core::ObjectId objectId,
  const dunya::field::Primitive& primitive
) {
  if (!contains(objectId)) {
    return false;
  }

  return insertPrimitive(
    objectId,
    m_fieldObjects[objectId].primitiveCount,
    primitive
  );
}

bool ObjectRegistry::removePrimitive(
  dunya::core::ObjectId objectId,
  uint32_t primitiveIndex
) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (primitiveIndex >= slot.primitiveCount) {
    return false;
  }

  const uint32_t begin = slot.primitiveOffset + primitiveIndex;

  const uint32_t end = slot.primitiveOffset + slot.primitiveCount;

  // Preserve CSG order.
  //
  // We cannot swap-and-pop primitives because your primitive order is
  // semantically meaningful to the CSG fold.
  std::move(
    m_primitives.begin() + begin + 1,
    m_primitives.begin() + end,
    m_primitives.begin() + begin
  );

  --slot.primitiveCount;

  FieldObject& object = *slot.object;

  object.dirty = true;

  refreshDerived(object, getPrimitives(objectId));

  return true;
}

void ObjectRegistry::clearPrimitives(dunya::core::ObjectId objectId) {
  if (!contains(objectId)) {
    return;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  slot.primitiveCount = 0;

  FieldObject& object = *slot.object;

  object.dirty = true;

  refreshDerived(object, getPrimitives(objectId));
}

FieldObject& ObjectRegistry::getFieldObject(dunya::core::ObjectId objectId) {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return *m_fieldObjects[objectId].object;
}

const FieldObject& ObjectRegistry::getFieldObject(
  dunya::core::ObjectId objectId
) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return *m_fieldObjects[objectId].object;
}

std::span<dunya::field::Primitive> ObjectRegistry::getPrimitives(
  dunya::core::ObjectId objectId
) {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  const FieldObjectSlot& slot = m_fieldObjects[objectId];

  return {m_primitives.data() + slot.primitiveOffset, slot.primitiveCount};
}

std::span<const dunya::field::Primitive> ObjectRegistry::getPrimitives(
  dunya::core::ObjectId objectId
) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  const FieldObjectSlot& slot = m_fieldObjects[objectId];

  return {m_primitives.data() + slot.primitiveOffset, slot.primitiveCount};
}

std::span<const dunya::core::ObjectId> ObjectRegistry::
  fieldObjectIds() const noexcept {
  return m_activeFieldObjects;
}

bool ObjectRegistry::contains(dunya::core::ObjectId objectId) const noexcept {
  return objectId < m_fieldObjects.size()
         && m_fieldObjects[objectId].object.has_value();
}

uint32_t ObjectRegistry::fieldObjectCount() const noexcept {
  return static_cast<uint32_t>(m_activeFieldObjects.size());
}

uint32_t ObjectRegistry::primitiveCount(dunya::core::ObjectId objectId) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return m_fieldObjects[objectId].primitiveCount;
}

std::span<const dunya::field::Primitive> ObjectRegistry::
  primitivePool() const noexcept {
  return m_primitives;
}

uint32_t ObjectRegistry::primitiveOffset(dunya::core::ObjectId objectId) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return m_fieldObjects[objectId].primitiveOffset;
}

}  // namespace dunya::objectmodel
