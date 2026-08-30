#include "fieldresidency.ih"

namespace dunya::renderer {

FieldResidency::FieldResidency(
  VolumePool& pool,
  FieldRecordTable& table,
  gpu::Uploader& uploader
)
    : m_pool(pool), m_table(table), m_uploader(uploader) {}

void FieldResidency::reclaim(const objectmodel::World& world) {
  const entt::registry& registry = world.registry();

  for (size_t held = 0; held != m_holders.size();) {
    const auto [owner, slot] = m_holders[held];

    if (
      registry.valid(owner) && registry.all_of<objectmodel::BakedVolume>(owner)
    ) {
      ++held;

      continue;
    }

    m_pool.release(slot);

    m_holders[held] = m_holders.back();
    m_holders.pop_back();
  }
}

objectmodel::Entity FieldResidency::fieldOnSlot(
  const objectmodel::World& world,
  uint32_t slot
) const {
  for (const auto& [owner, held] : m_holders) {
    if (held != slot || !world.registry().valid(owner)) {
      continue;
    }

    if (world.hasSampledField(owner)) {
      return owner;
    }
  }

  return objectmodel::INVALID_ENTITY;
}

void FieldResidency::hold(objectmodel::Entity entity, uint32_t slot) {
  m_holders.emplace_back(entity, slot);
}

void FieldResidency::releaseAll(objectmodel::World& world) {
  for (const auto& [owner, slot] : m_holders) {
    if (world.registry().valid(owner)) {
      world.clearBakedVolume(owner);
    }

    m_pool.release(slot);
  }

  m_holders.clear();
}

uint32_t FieldResidency::upload(
  objectmodel::World& world,
  std::span<const std::pair<objectmodel::Entity, field::SampleBox>> dirty
) {
  if (dirty.empty()) {
    return 0u;
  }

  const entt::registry& registry = world.registry();

  uint32_t dropped = 0u;

  for (const auto& [entity, box] : dirty) {
    if (
      !registry.valid(entity)
      || !registry.all_of<objectmodel::BakedVolume, objectmodel::SharedField>(
        entity
      )
    ) {
      continue;
    }

    const uint32_t shared =
      registry.get<objectmodel::BakedVolume>(entity).index;

    const field::SampledField& field = *world.sampledField(entity);

    const uint32_t slot = m_pool.makeUnique(m_uploader, shared, field);

    if (slot == UINT32_MAX) {
      ++dropped;

      continue;
    }

    if (slot != shared) {
      m_pool.release(shared);

      const auto held = std::find(
        m_holders.begin(),
        m_holders.end(),
        std::pair<objectmodel::Entity, uint32_t>{entity, shared}
      );

      if (held != m_holders.end()) {
        held->second = slot;
      }

      const auto images = m_pool.images(slot);

      m_table.registerVolume(
        images.distance.imageView(),
        images.material.imageView(),
        slot
      );

      world.setBakedVolume(entity, slot);
    }

    m_pool.upload(m_uploader, slot, field, box);

    m_table.uploadBounds(m_uploader, slot, field);
  }

  m_uploader.submit();

  return dropped;
}

}
