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

// Which entity holds which volume slot, and the three rules that keep the two
// agreeing: a slot goes back when its entity does, a slot is found again when
// another object wants the lattice already on it, and a slot is split when the
// object that shares it is damaged.
//
// It lives here rather than inside VolumePool because the pool is a renderer
// resource and the registry is world state - and it lives outside the frame
// loop because the rules do not change with how the frame is arranged.
//
// A list of references rather than a slot-indexed array, because a slot is
// shared: a thousand identical crates hold one volume between them.
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

  // Slots whose entity is gone, or which has lost its BakedVolume, go back -
  // before anything asks for a new one.
  void reclaim(const objectmodel::World& world);

  // An object already holding a lattice on this slot, or INVALID_ENTITY if
  // none does. What lets an object take a shared volume without baking a
  // second identical copy of what filled it, and then share that lattice
  // rather than copy it.
  [[nodiscard]] objectmodel::Entity fieldOnSlot(
    const objectmodel::World& world,
    uint32_t slot
  ) const;

  void hold(objectmodel::Entity entity, uint32_t slot);

  // Every slot goes back on a world switch: the sampled field is rebuilt for
  // the new world rather than carried across.
  void releaseAll(objectmodel::World& world);

  // Copy on write, plus the copy itself. Each changed region is sent, and a
  // shared volume is split first so a dent lands on one object rather than on
  // every object that shares its geometry. One submission for the frame and no
  // wait: the rendering follows on the same queue and the barriers order
  // against it.
  //
  // Returns how many dents could not be drawn because the pool was full, so
  // the caller can say so once rather than once a frame.
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
