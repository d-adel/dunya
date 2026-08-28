#include "world.ih"

namespace dunya::objectmodel {

namespace {

// Named, because the point of reactive storage over a flag is that a second
// consumer opens its own pool rather than sharing this one.
constexpr entt::id_type BAKE_QUEUE = entt::hashed_string{"bake"};

}  // namespace

World::World() {
  m_primitiveStore.connect(m_registry);

  // on_construct as well as on_update: the dirty flag this replaced defaulted
  // to true, so a field entity has always needed its first bake on creation.
  // on_update fires from registry.patch, which is why the store patches.
  m_registry.storage<entt::reactive>(BAKE_QUEUE)
    .on_construct<SdfGrid>()
    .on_update<SdfGrid>();
}

const entt::registry& World::registry() const noexcept {
  return m_registry;
}

Entity World::createField(const Pose& pose, const SdfGrid& grid) {
  const Entity entity = m_registry.create();

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<SdfGrid>(entity, grid);

  return entity;
}

bool World::createFieldAt(Entity hint, const Pose& pose, const SdfGrid& grid) {
  const Entity entity = m_registry.create(hint);

  // EnTT treats a hint as a request, not a requirement. For undo/redo that is
  // failure, because identity is part of the operation.
  if (entity != hint) {
    m_registry.destroy(entity);
    return false;
  }

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<SdfGrid>(entity, grid);

  return true;
}

bool World::destroyField(Entity entity) {
  if (!m_registry.valid(entity) || !m_registry.all_of<SdfGrid>(entity)) {
    return false;
  }

  // If the entity has an SdfPrimitiveRange, destroying it publishes
  // on_destroy<SdfPrimitiveRange> before removing the component. The
  // primitive store receives that signal and releases the arena range.
  m_registry.destroy(entity);

  return true;
}

std::span<const Entity> World::fields() const noexcept {
  const auto* storage = m_registry.storage<SdfGrid>();

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
  Pose& target = m_registry.get<Pose>(entity);

  target.position = position;
  target.rotation = rotation;
}

void World::setBakedVolume(Entity entity, uint32_t index) {
  m_registry.emplace_or_replace<BakedVolume>(entity, index);
}

bool World::needsBake(Entity entity) const noexcept {
  const auto* queue = m_registry.storage<entt::reactive>(BAKE_QUEUE);

  return queue != nullptr && queue->contains(entity);
}

void World::markBaked(Entity entity) {
  // remove, not erase: erase asserts on an entity the queue never held, and
  // the caller re-derives its list from packing slots rather than from here.
  m_registry.storage<entt::reactive>(BAKE_QUEUE).remove(entity);
}

Entity World::createMesh(
  const Pose& pose,
  const Mesh& mesh,
  const Material& material
) {
  const Entity entity = m_registry.create();

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<Mesh>(entity, mesh);
  m_registry.emplace<Material>(entity, material);

  return entity;
}

std::span<const Entity> World::meshes() const noexcept {
  const auto* storage = m_registry.storage<Mesh>();

  if (storage == nullptr) {
    return {};
  }

  return {storage->data(), storage->size()};
}

}  // namespace dunya::objectmodel
