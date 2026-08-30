#pragma once

#include <dunya/field/sampled/sampled.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/fieldrecordtable/fieldrecordtable.h>
#include <dunya/renderer/volumepool/volumepool.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace dunya::renderer {

class FieldResidency {
public:
  FieldResidency(
    VolumePool& pool,
    FieldRecordTable& table,
    gpu::Uploader& uploader
  );

  FieldResidency(const FieldResidency&) = delete;
  FieldResidency& operator=(const FieldResidency&) = delete;
  FieldResidency(FieldResidency&&) = delete;
  FieldResidency& operator=(FieldResidency&&) = delete;

  void reclaim(const objectmodel::World& world);

  [[nodiscard]] objectmodel::Entity fieldOnSlot(
    const objectmodel::World& world,
    uint32_t slot
  ) const;

  void hold(objectmodel::Entity entity, uint32_t slot);

  void releaseAll(objectmodel::World& world);

  uint32_t upload(
    objectmodel::World& world,
    std::span<const std::pair<objectmodel::Entity, field::SampleBox>> dirty
  );

private:
  VolumePool& m_pool;
  FieldRecordTable& m_table;
  gpu::Uploader& m_uploader;

  std::vector<std::pair<objectmodel::Entity, uint32_t>> m_holders;
};

}  // namespace dunya::renderer
