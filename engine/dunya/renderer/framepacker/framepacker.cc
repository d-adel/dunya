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
  m_packed = &world;

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

  const dunya::objectmodel::Entity sun =
    dunya::objectmodel::firstWith<dunya::objectmodel::DirectionalLight>(world);

  if (sun != dunya::objectmodel::INVALID_ENTITY) {
    frame.light =
      world.registry().get<const dunya::objectmodel::DirectionalLight>(sun);
  }

  const dunya::objectmodel::Entity sky =
    dunya::objectmodel::firstWith<dunya::objectmodel::Environment>(world);

  if (sky != dunya::objectmodel::INVALID_ENTITY) {
    frame.environment =
      world.registry().get<const dunya::objectmodel::Environment>(sky);
  }
}

void FramePacker::commitBakes() {
  if (m_packed == nullptr) {
    return;
  }

  const std::span<const dunya::objectmodel::Entity> packed =
    m_sdfRecords.recordEntities();

  for (const uint32_t slot : m_recordTable.bakeList()) {
    m_packed->markBaked(packed[slot]);
  }

  m_packed = nullptr;
}

std::span<const dunya::objectmodel::Entity> FramePacker::
  sdfRecordEntities() const noexcept {
  return m_sdfRecords.recordEntities();
}

}
