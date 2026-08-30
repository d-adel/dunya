#include "framepacker.ih"

namespace dunya::renderer {

FramePacker::FramePacker(
  VolumePool& volumePool,
  SdfResidency& residency,
  SdfRecordTable& recordTable
)
    : m_recordTable(recordTable),
      m_sdfRecords(volumePool, residency, recordTable) {}

void FramePacker::pack(
  Frame& frame,
  dunya::objectmodel::World& world,
  std::span<const dunya::objectmodel::Entity> sdfGrids,
  std::span<const MeshBuffers> meshBuffers,
  const std::function<void(dunya::objectmodel::Entity)>& onFieldReplaced
) {
  m_recordTable.newFrame();

  frame.sdfRecordCount = m_sdfRecords.pack(
    world,
    sdfGrids,
    frame.fieldRepresentation,
    onFieldReplaced
  );

  frame.meshRecords = m_meshRecords.pack(world, world.meshes());
  frame.meshes = meshBuffers;
  frame.primitives = world.pool();
}

std::span<const dunya::objectmodel::Entity> FramePacker::
  sdfRecordEntities() const noexcept {
  return m_sdfRecords.recordEntities();
}

}
