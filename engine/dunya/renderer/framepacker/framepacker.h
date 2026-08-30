#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/sdfrecordpacker/sdfrecordpacker.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/renderer/meshrecordpacker/meshrecordpacker.h>

#include <functional>
#include <span>

namespace dunya::renderer {

class FramePacker {
public:
  FramePacker(
    VolumePool& volumePool,
    SdfResidency& residency,
    SdfRecordTable& recordTable
  );

  FramePacker(const FramePacker&) = delete;
  FramePacker& operator=(const FramePacker&) = delete;
  FramePacker(FramePacker&&) = delete;
  FramePacker& operator=(FramePacker&&) = delete;

  void pack(
    Frame& frame,
    dunya::objectmodel::World& world,
    std::span<const dunya::objectmodel::Entity> sdfGrids,
    std::span<const MeshBuffers> meshBuffers,
    const std::function<void(dunya::objectmodel::Entity)>& onFieldReplaced = {}
  );

  [[nodiscard]] std::span<const dunya::objectmodel::Entity>
  sdfRecordEntities() const noexcept;

private:
  SdfRecordTable& m_recordTable;
  SdfRecordPacker m_sdfRecords;
  MeshRecordPacker m_meshRecords;
};

}
