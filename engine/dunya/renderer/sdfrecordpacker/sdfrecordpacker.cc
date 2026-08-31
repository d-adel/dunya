#include "sdfrecordpacker.ih"

namespace dunya::renderer {

SdfRecordPacker::SdfRecordPacker(
  VolumePool& volumePool,
  SdfResidency& residency,
  SdfRecordTable& recordTable
)
    : m_volumePool(volumePool),
      m_residency(residency),
      m_recordTable(recordTable) {}

uint32_t SdfRecordPacker::pack(
  dunya::objectmodel::World& world,
  std::span<const dunya::objectmodel::Entity> entities,
  uint32_t fieldRepresentation,
  const std::function<void(dunya::objectmodel::Entity)>& onFieldReplaced
) {
  const entt::registry& registry = world.registry();

  m_residency.reclaim(world);

  uint32_t recordIndex = 0;

  m_recordEntities.clear();

  for (const dunya::objectmodel::Entity entity : entities) {
    if (recordIndex == dunya::core::MAX_SDF_RECORDS) {
      if (!m_tableFullReported) {
        m_tableFullReported = true;
        std::cout << "Sdf record table full, the rest are not drawn\n";
      }

      break;
    }

    const dunya::objectmodel::SdfGrid& grid =
      registry.get<dunya::objectmodel::SdfGrid>(entity);

    if (!registry.all_of<dunya::objectmodel::BakedVolume>(entity)) {
      std::span<const dunya::field::Primitive> primitives =
        world.primitives(entity);

      const dunya::field::SampledSdf* carried = world.sampledSdf(entity);

      const bool reusable = carried != nullptr && !world.needsResample(entity);

      const VolumeKey key =
        registry.all_of<dunya::objectmodel::Deformed>(entity)
          ? VolumeKey{}
          : volumeKey(primitives, grid.resolution, grid.margin);

      uint32_t index = reusable ? UINT32_MAX : m_volumePool.acquire(key);

      const dunya::objectmodel::Entity donor =
        index == UINT32_MAX ? dunya::objectmodel::INVALID_ENTITY
                            : m_residency.sdfOnSlot(world, index);

      if (index != UINT32_MAX && donor == dunya::objectmodel::INVALID_ENTITY) {
        m_volumePool.release(index);

        index = UINT32_MAX;
      }

      dunya::field::SampledSdf baked;

      if (!reusable && donor == dunya::objectmodel::INVALID_ENTITY) {
        const dunya::field::Aabb box =
          dunya::objectmodel::gridBox(primitives, grid.margin);

        baked = dunya::field::bake(
          primitives,
          box.minimum,
          box.maximum,
          grid.resolution
        );
      }

      if (index == UINT32_MAX) {
        index = m_volumePool.allocate(reusable ? *carried : baked, key);
      }

      if (index == UINT32_MAX) {
        if (!m_volumePoolFullReported) {
          m_volumePoolFullReported = true;
          std::cout << "Volume pool full, object not drawn\n";
        }

        continue;
      }

      auto images = m_volumePool.images(index);

      m_recordTable.registerVolume(
        images.distance.imageView(),
        images.material.imageView(),
        index
      );

      m_residency.hold(entity, index);

      world.setBakedVolume(entity, index);

      if (donor != dunya::objectmodel::INVALID_ENTITY) {
        world.shareSampledSdf(donor, entity);
      } else if (!reusable) {
        world.setSampledSdf(entity, std::move(baked));
      }

      if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
        m_recordTable.uploadBounds(index, *world.sampledSdf(entity));
      }

      if (onFieldReplaced) {
        onFieldReplaced(entity);
      }
    }

    const auto* range =
      registry.try_get<dunya::objectmodel::SdfPrimitiveRange>(entity);

    const uint32_t primitiveOffset = range == nullptr ? 0u : range->offset;

    const uint32_t primitiveCount = range == nullptr ? 0u : range->count;

    m_recordTable.setRecord(
      recordIndex,
      primitiveOffset,
      primitiveCount,
      dunya::objectmodel::drawnPose(registry, entity),
      grid,
      registry.get<dunya::objectmodel::BakedVolume>(entity),
      fieldRepresentation
    );

    if (world.needsBake(entity)) {
      if (registry.all_of<dunya::objectmodel::Deformable>(entity)) {
        world.markBaked(entity);
      } else {
        m_recordTable.appendToBakeList(recordIndex);

        if (
          onFieldReplaced && world.needsResample(entity)
          && registry.all_of<
             dunya::objectmodel::RigidBody,
             dunya::objectmodel::SharedSdf>(entity)
        ) {
          const dunya::field::Aabb refit =
            dunya::objectmodel::gridBox(world.primitives(entity), grid.margin);

          world.setSampledSdf(
            entity,
            dunya::field::bake(
              world.primitives(entity),
              refit.minimum,
              refit.maximum,
              grid.resolution
            )
          );

          onFieldReplaced(entity);
        }
      }
    }

    m_recordEntities.push_back(entity);
    ++recordIndex;
  }

  return recordIndex;
}

std::span<const dunya::objectmodel::Entity> SdfRecordPacker::
  recordEntities() const noexcept {
  return m_recordEntities;
}

}
