#include "world.ih"

namespace dunya::objectmodel {

namespace {

// Named, because the point of reactive storage over a flag is that a second
// consumer opens its own pool rather than sharing this one.
constexpr entt::id_type BAKE_QUEUE = entt::hashed_string{"bake"};

// Its own queue, and the reason is that it answers a different question: the
// one above says the GPU volume is stale, this one says the CPU field is.
constexpr entt::id_type RESAMPLE_QUEUE = entt::hashed_string{"resample"};

}  // namespace

World::World() {
  m_primitiveStore.connect(m_registry);

  // on_construct as well as on_update: the dirty flag this replaced defaulted
  // to true, so a field entity has always needed its first bake on creation.
  // on_update fires from registry.patch, which is why the store patches.
  m_registry.storage<entt::reactive>(BAKE_QUEUE)
    .on_construct<SdfGrid>()
    .on_update<SdfGrid>();

  m_registry.storage<entt::reactive>(RESAMPLE_QUEUE)
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

void World::addStaticBody(Entity entity) {
  m_registry.emplace_or_replace<StaticBody>(entity);
}

void World::removeStaticBody(Entity entity) {
  m_registry.remove<StaticBody>(entity);
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

void World::setBakedVolume(Entity entity, uint32_t index) {
  m_registry.emplace_or_replace<BakedVolume>(entity, index);
}

void World::setRigidBody(Entity entity, uint32_t index) {
  m_registry.emplace_or_replace<RigidBody>(entity, index);
}

void World::setSampledField(Entity entity, dunya::field::SampledField field) {
  m_registry.emplace_or_replace<SharedField>(
    entity,
    std::make_shared<dunya::field::SampledField>(std::move(field))
  );

  // Setting the field is what makes it current, so this is where the queue
  // that tracks staleness is answered.
  m_registry.storage<entt::reactive>(RESAMPLE_QUEUE).remove(entity);

  // And a field that came from the primitives is derived from them again,
  // whatever the one it replaced had been through.
  m_registry.remove<Deformed>(entity);
}

void World::shareSampledField(Entity donor, Entity taker) {
  const auto* held = m_registry.try_get<SharedField>(donor);

  if (held == nullptr) {
    throw std::runtime_error("Sharing a lattice from an object without one");
  }

  // By value, not by reference: emplace_or_replace may move the donor's own
  // component while the taker's is being written.
  adoptSampledField(taker, SharedField{held->field});
}

void World::adoptSampledField(Entity entity, const SharedField& held) {
  if (held.field == nullptr) {
    throw std::runtime_error("Adopting an empty lattice handle");
  }

  m_registry.emplace_or_replace<SharedField>(entity, held);

  // Current, because a lattice that exists is one somebody baked. Deformed is
  // deliberately not touched: whether this lattice has been written in place
  // is a property of the lattice, and the handle does not carry it - the
  // caller knows which it is handing over.
  m_registry.storage<entt::reactive>(RESAMPLE_QUEUE).remove(entity);
}

const dunya::field::SampledField* World::sampledField(Entity entity) const {
  const auto* held = m_registry.try_get<SharedField>(entity);

  return held == nullptr ? nullptr : held->field.get();
}

bool World::hasSampledField(Entity entity) const noexcept {
  return m_registry.all_of<SharedField>(entity);
}

long World::sampledFieldUsers(Entity entity) const noexcept {
  const auto* held = m_registry.try_get<SharedField>(entity);

  return held == nullptr ? 0 : held->field.use_count();
}

void World::clearBakedVolume(Entity entity) {
  m_registry.remove<BakedVolume>(entity);
}

bool World::needsBake(Entity entity) const noexcept {
  const auto* queue = m_registry.storage<entt::reactive>(BAKE_QUEUE);

  return queue != nullptr && queue->contains(entity);
}

bool World::needsResample(Entity entity) const noexcept {
  const auto* queue = m_registry.storage<entt::reactive>(RESAMPLE_QUEUE);

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

bool World::createMeshAt(
  Entity hint,
  const Pose& pose,
  const Mesh& mesh,
  const Material& material
) {
  const Entity entity = m_registry.create(hint);

  // Same contract as createFieldAt: a hint EnTT declines is a failure,
  // because instantiation keys physics and volumes off the id.
  if (entity != hint) {
    m_registry.destroy(entity);
    return false;
  }

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<Mesh>(entity, mesh);
  m_registry.emplace<Material>(entity, material);

  return true;
}

std::span<const Entity> World::meshes() const noexcept {
  const auto* storage = m_registry.storage<Mesh>();

  if (storage == nullptr) {
    return {};
  }

  return {storage->data(), storage->size()};
}

}  // namespace dunya::objectmodel
