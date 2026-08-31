#include "world.ih"

namespace dunya::objectmodel {

namespace {

constexpr entt::id_type BAKE_QUEUE = entt::hashed_string{"bake"};

constexpr entt::id_type RESAMPLE_QUEUE = entt::hashed_string{"resample"};

}

World::World() {
  m_primitiveStore.connect(m_registry);

  m_registry.on_destroy<BakedVolume>().connect<&World::bakedVolumeDestroyed>(
    *this
  );

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

void World::clear() {
  m_sdfDirty.clear();
  m_dynamic.clearEntities();
  m_registry.clear();
}

DynamicComponents& World::dynamic() noexcept {
  return m_dynamic;
}

const DynamicComponents& World::dynamic() const noexcept {
  return m_dynamic;
}

Entity World::createAuthored() {
  return m_registry.create();
}

bool World::createAuthoredAt(Entity hint) {
  const Entity entity = m_registry.create(hint);

  if (entity != hint) {
    m_registry.destroy(entity);
    return false;
  }

  return true;
}

Entity World::createSdfGrid(const Pose& pose, const SdfGrid& grid) {
  const Entity entity = m_registry.create();

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<SdfGrid>(entity, grid);

  return entity;
}

bool World::createSdfGridAt(
  Entity hint,
  const Pose& pose,
  const SdfGrid& grid
) {
  const Entity entity = m_registry.create(hint);

  if (entity != hint) {
    m_registry.destroy(entity);
    return false;
  }

  m_registry.emplace<Pose>(entity, pose);
  m_registry.emplace<SdfGrid>(entity, grid);

  return true;
}

bool World::destroy(Entity entity) {
  if (!m_registry.valid(entity)) {
    return false;
  }

  m_dynamic.clear(entity);

  m_registry.destroy(entity);

  return true;
}

std::span<const Entity> World::fields() const noexcept {
  return sdfGrids();
}

std::span<const Entity> World::sdfGrids() const noexcept {
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
  m_registry.remove<BakedVolume>(entity);
  m_registry.emplace<BakedVolume>(entity, index);
}

void World::setRigidBody(Entity entity, uint32_t index) {
  m_registry.emplace_or_replace<RigidBody>(entity, index);
}

void World::setSampledSdf(Entity entity, dunya::field::SampledSdf field) {
  m_registry.emplace_or_replace<SharedSdf>(
    entity,
    std::make_shared<dunya::field::SampledSdf>(std::move(field))
  );

  m_registry.storage<entt::reactive>(RESAMPLE_QUEUE).remove(entity);

  m_registry.remove<Deformed>(entity);
}

void World::shareSampledSdf(Entity donor, Entity taker) {
  const auto* held = m_registry.try_get<SharedSdf>(donor);

  if (held == nullptr) {
    throw std::runtime_error("Sharing a lattice from an object without one");
  }

  adoptSampledSdf(taker, SharedSdf{held->field});
}

void World::adoptSampledSdf(Entity entity, const SharedSdf& held) {
  if (held.field == nullptr) {
    throw std::runtime_error("Adopting an empty lattice handle");
  }

  m_registry.emplace_or_replace<SharedSdf>(entity, held);

  m_registry.storage<entt::reactive>(RESAMPLE_QUEUE).remove(entity);
}

const dunya::field::SampledSdf* World::sampledSdf(Entity entity) const {
  const auto* held = m_registry.try_get<SharedSdf>(entity);

  return held == nullptr ? nullptr : held->field.get();
}

bool World::hasSampledSdf(Entity entity) const noexcept {
  return m_registry.all_of<SharedSdf>(entity);
}

long World::sampledSdfUsers(Entity entity) const noexcept {
  const auto* held = m_registry.try_get<SharedSdf>(entity);

  return held == nullptr ? 0 : held->field.use_count();
}

void World::clearBakedVolume(Entity entity) {
  m_registry.remove<BakedVolume>(entity);
}

void World::clearBakedVolumes() {
  for (const Entity entity : std::vector<Entity>(
         m_registry.view<BakedVolume>().begin(),
         m_registry.view<BakedVolume>().end()
       )) {
    m_registry.remove<BakedVolume>(entity);
  }
}

void World::onBakedVolumeReleased(std::function<void(uint32_t)> release) {
  m_onBakedVolumeReleased = std::move(release);
}

void World::bakedVolumeDestroyed(entt::registry& registry, Entity entity) {
  if (m_onBakedVolumeReleased) {
    m_onBakedVolumeReleased(registry.get<BakedVolume>(entity).index);
  }
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

void World::markSdfDirty(Entity entity, const dunya::field::SampleBox& box) {
  const auto found =
    std::find_if(m_sdfDirty.begin(), m_sdfDirty.end(), [entity](const auto& e) {
      return e.first == entity;
    });

  if (found == m_sdfDirty.end()) {
    m_sdfDirty.emplace_back(entity, box);
  } else {
    found->second = dunya::field::merge(found->second, box);
  }
}

std::span<const std::pair<Entity, dunya::field::SampleBox>> World::
  sdfDirty() const noexcept {
  return m_sdfDirty;
}

void World::clearSdfDirty() noexcept {
  m_sdfDirty.clear();
}

}
