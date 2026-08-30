#pragma once

#include <dunya/field/sampledsdf/sampledsdf.h>
#include <dunya/core/telemetry/telemetry.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/volumepool/volumepool.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace dunya::renderer {

class SdfResidency {
public:
  SdfResidency(
    VolumePool& pool,
    SdfRecordTable& table,
    gpu::Uploader& uploader
  );

  SdfResidency(const SdfResidency&) = delete;
  SdfResidency& operator=(const SdfResidency&) = delete;
  SdfResidency(SdfResidency&&) = delete;
  SdfResidency& operator=(SdfResidency&&) = delete;

  void reclaim(const objectmodel::World& world);

  [[nodiscard]] objectmodel::Entity sdfOnSlot(
    const objectmodel::World& world,
    uint32_t slot
  ) const;

  void hold(objectmodel::Entity entity, uint32_t slot);

  void releaseAll(objectmodel::World& world);

  void upload(
    objectmodel::World& world,
    std::span<const std::pair<objectmodel::Entity, field::SampleBox>> dirty,
    core::Telemetry& telemetry
  );

private:
  VolumePool& m_pool;
  SdfRecordTable& m_table;
  gpu::Uploader& m_uploader;

  std::vector<std::pair<objectmodel::Entity, uint32_t>> m_holders;
};

}
