#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/sdfresidency/sdfresidency.h>
#include <dunya/renderer/volumepool/volumepool.h>

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace dunya::renderer {

class SdfRecordPacker {
public:
  SdfRecordPacker(
    VolumePool& volumePool,
    SdfResidency& residency,
    SdfRecordTable& recordTable
  );

  SdfRecordPacker(const SdfRecordPacker&) = delete;
  SdfRecordPacker& operator=(const SdfRecordPacker&) = delete;
  SdfRecordPacker(SdfRecordPacker&&) = delete;
  SdfRecordPacker& operator=(SdfRecordPacker&&) = delete;

  [[nodiscard]] uint32_t pack(
    dunya::objectmodel::World& world,
    std::span<const dunya::objectmodel::Entity> entities,
    uint32_t fieldRepresentation,
    const std::function<void(dunya::objectmodel::Entity)>& onFieldReplaced = {}
  );

  [[nodiscard]] std::span<const dunya::objectmodel::Entity>
  recordEntities() const noexcept;

private:
  VolumePool& m_volumePool;
  SdfResidency& m_residency;
  SdfRecordTable& m_recordTable;

  std::vector<dunya::objectmodel::Entity> m_recordEntities;

  bool m_tableFullReported = false;
  bool m_volumePoolFullReported = false;
};

}
