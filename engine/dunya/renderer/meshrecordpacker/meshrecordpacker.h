#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/meshrecord/meshrecord.h>

#include <span>
#include <vector>

namespace dunya::renderer {

class MeshRecordPacker {
public:
  MeshRecordPacker() = default;

  MeshRecordPacker(const MeshRecordPacker&) = delete;
  MeshRecordPacker& operator=(const MeshRecordPacker&) = delete;
  MeshRecordPacker(MeshRecordPacker&&) = delete;
  MeshRecordPacker& operator=(MeshRecordPacker&&) = delete;

  [[nodiscard]] std::span<const MeshRecord> pack(
    const dunya::objectmodel::World& world,
    std::span<const dunya::objectmodel::Entity> entities
  );

private:
  std::vector<MeshRecord> m_records;
};

}
