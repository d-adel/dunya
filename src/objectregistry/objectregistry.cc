#include "objectregistry.ih"

using dunya::field::Primitive;

namespace {

constexpr uint32_t INITIAL_PRIMITIVE_CAPACITY = 4;

}

ObjectRegistry::ObjectRegistry() : m_fieldObjects(MAX_FIELD_OBJECTS) {
  m_activeFieldObjects.reserve(MAX_FIELD_OBJECTS);
}

ObjectId ObjectRegistry::allocateObjectId() {
  if (!m_freeObjectIds.empty()) {
    const ObjectId objectId = m_freeObjectIds.top();
    m_freeObjectIds.pop();

    return objectId;
  }

  if (m_nextUnusedObjectId >= MAX_FIELD_OBJECTS) {
    return INVALID_OBJECT_ID;
  }

  return m_nextUnusedObjectId++;
}

ObjectId ObjectRegistry::addFieldObject(const FieldObject& fieldObject) {
  const ObjectId objectId = allocateObjectId();

  if (objectId == INVALID_OBJECT_ID) {
    return INVALID_OBJECT_ID;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  slot.object = fieldObject;

  slot.primitiveOffset = 0;
  slot.primitiveCount = 0;
  slot.primitiveCapacity = 0;

  slot.activeIndex = static_cast<uint32_t>(m_activeFieldObjects.size());

  m_activeFieldObjects.push_back(objectId);

  return objectId;
}

bool ObjectRegistry::removeFieldObject(ObjectId objectId) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (slot.primitiveCapacity > 0) {
    releasePrimitiveRange(slot.primitiveOffset, slot.primitiveCapacity);
  }

  const uint32_t removedIndex = slot.activeIndex;
  const ObjectId lastObjectId = m_activeFieldObjects.back();

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

  if (m_primitives.size() + capacity > MAX_PRIMITIVE_POOL) {
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
  if (requiredCapacity > MAX_FIELD_PRIMITIVES) {
    return std::nullopt;
  }

  uint32_t capacity =
    currentCapacity == 0
      ? std::min(INITIAL_PRIMITIVE_CAPACITY, MAX_FIELD_PRIMITIVES)
      : currentCapacity;

  while (capacity < requiredCapacity) {
    if (capacity >= MAX_FIELD_PRIMITIVES) {
      return std::nullopt;
    }

    capacity = std::min(capacity * 2, MAX_FIELD_PRIMITIVES);
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
  ObjectId objectId,
  const dunya::field::Primitive& primitive
) {
  if (!contains(objectId)) {
    return false;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  if (slot.primitiveCount >= MAX_FIELD_PRIMITIVES) {
    return false;
  }

  if (slot.primitiveCount == slot.primitiveCapacity) {
    if (!growPrimitiveRange(slot, slot.primitiveCount + 1)) {
      return false;
    }
  }

  m_primitives[slot.primitiveOffset + slot.primitiveCount] = primitive;

  ++slot.primitiveCount;

  FieldObject& object = *slot.object;

  object.dirty = true;

  refreshDerived(object, getPrimitives(objectId));

  return true;
}

bool ObjectRegistry::removePrimitive(
  ObjectId objectId,
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

void ObjectRegistry::clearPrimitives(ObjectId objectId) {
  if (!contains(objectId)) {
    return;
  }

  FieldObjectSlot& slot = m_fieldObjects[objectId];

  slot.primitiveCount = 0;

  FieldObject& object = *slot.object;

  object.dirty = true;

  refreshDerived(object, getPrimitives(objectId));
}

FieldObject& ObjectRegistry::getFieldObject(ObjectId objectId) {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return *m_fieldObjects[objectId].object;
}

const FieldObject& ObjectRegistry::getFieldObject(ObjectId objectId) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return *m_fieldObjects[objectId].object;
}

std::span<dunya::field::Primitive> ObjectRegistry::getPrimitives(
  ObjectId objectId
) {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  const FieldObjectSlot& slot = m_fieldObjects[objectId];

  return {m_primitives.data() + slot.primitiveOffset, slot.primitiveCount};
}

std::span<const dunya::field::Primitive> ObjectRegistry::getPrimitives(
  ObjectId objectId
) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  const FieldObjectSlot& slot = m_fieldObjects[objectId];

  return {m_primitives.data() + slot.primitiveOffset, slot.primitiveCount};
}

std::span<const ObjectId> ObjectRegistry::fieldObjectIds() const noexcept {
  return m_activeFieldObjects;
}

bool ObjectRegistry::contains(ObjectId objectId) const noexcept {
  return objectId < m_fieldObjects.size()
         && m_fieldObjects[objectId].object.has_value();
}

uint32_t ObjectRegistry::fieldObjectCount() const noexcept {
  return static_cast<uint32_t>(m_activeFieldObjects.size());
}

uint32_t ObjectRegistry::primitiveCount(ObjectId objectId) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return m_fieldObjects[objectId].primitiveCount;
}

std::span<const dunya::field::Primitive> ObjectRegistry::
  primitivePool() const noexcept {
  return m_primitives;
}

uint32_t ObjectRegistry::primitiveOffset(ObjectId objectId) const {
  if (!contains(objectId)) {
    throw std::out_of_range("Invalid ObjectId");
  }

  return m_fieldObjects[objectId].primitiveOffset;
}
