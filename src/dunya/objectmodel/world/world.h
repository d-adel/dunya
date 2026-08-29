#pragma once

#include <dunya/field/sampled/sampled.h>

#include <dunya/objectmodel/material/material.h>
#include <dunya/objectmodel/mesh/mesh.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>
#include <dunya/objectmodel/selfcontained/selfcontained.h>
#include <dunya/objectmodel/rigidbody/rigidbody.h>
#include <dunya/objectmodel/staticbody/staticbody.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// EnTT swaps its last element into a removed slot, which would move one
// entity's field out from under a collision shape holding another's. Leave a
// tombstone instead: the pool is walked by entity, never packed, so nothing
// pays for the hole. Declared here rather than on SampledField itself, which
// sits in a library that must not know EnTT exists.
template<>
struct entt::component_traits<dunya::field::SampledField, entt::entity> {
  using element_type = dunya::field::SampledField;
  using entity_type = entt::entity;

  static constexpr bool in_place_delete = true;
  static constexpr std::size_t page_size = ENTT_PACKED_PAGE;
};

namespace dunya::objectmodel {

class World {
public:
  World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;
  World(World&&) = delete;
  World& operator=(World&&) = delete;

  // Read-only from outside World.
  const entt::registry& registry() const noexcept;

  // Field-object lifetime.
  Entity createField(const Pose& pose, const SdfGrid& grid);

  [[nodiscard]]
  bool createFieldAt(Entity hint, const Pose& pose, const SdfGrid& grid);

  [[nodiscard]]
  bool destroyField(Entity entity);

  // Dense list of entities carrying SdfGrid.
  std::span<const Entity> fields() const noexcept;

  // Primitive transactions.
  [[nodiscard]]
  bool addPrimitive(Entity entity, const dunya::field::Primitive& primitive);

  [[nodiscard]]
  bool insertPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool setPrimitive(
    Entity entity,
    uint32_t index,
    const dunya::field::Primitive& primitive
  );

  [[nodiscard]]
  bool removePrimitive(Entity entity, uint32_t index);

  // A tag: added and removed, never assigned, so it takes neither a value nor
  // the generic mutations.
  void addStaticBody(Entity entity);
  void removeStaticBody(Entity entity);

  std::span<const dunya::field::Primitive> primitives(Entity entity) const;

  uint32_t primitiveCount(Entity entity) const;

  std::span<const dunya::field::Primitive> pool() const noexcept;

  // The one generic mutation: on_update fires by construction, so a new
  // self-contained component needs no new method here.
  template<SelfContained T, typename Fn>
  void patch(Entity entity, Fn&& fn) {
    m_registry.patch<T>(entity, std::forward<Fn>(fn));
  }

  // Whole-component replacement. EnTT's verb and EnTT's precondition: the
  // component has to be there already.
  template<SelfContained T>
  void replace(Entity entity, const T& value) {
    m_registry.replace<T>(entity, value);
  }

  template<SelfContained T>
  void replaceMany(std::span<const std::pair<Entity, T>> values) {
    for (const auto entity : values) {
      m_registry.replace<T>(entity.first, entity.second);
    }
  }

  // Owns a slot in the renderer's volume pool, so it is not self-contained
  // and keeps a method of its own.
  void setBakedVolume(Entity entity, uint32_t index);

  // Not self-contained
  void setRigidBody(Entity entity, uint32_t index);

  // The CPU-resident field, which physics queries for contacts. Baked from the
  // primitives, so it is not self-contained and only the bake may write it.
  void setSampledField(Entity entity, dunya::field::SampledField field);

  // Presence is the state, so giving a slot back means removing the
  // component, not writing a sentinel into it.
  void clearBakedVolume(Entity entity);

  // Change tracking, not a flag: the queue is entt::reactive storage filled
  // from the registry's own signals, so a new mutation path is covered without
  // anyone remembering to mark it.
  bool needsBake(Entity entity) const noexcept;
  void markBaked(Entity entity);

  // Whether the CPU field is stale. Its own queue rather than the one above,
  // because that is answered by a GPU bake and this one by setSampledField.
  bool needsResample(Entity entity) const noexcept;

  // Mesh lifetime, the same shape as the field one above.
  Entity createMesh(
    const Pose& pose,
    const Mesh& mesh,
    const Material& material
  );

  [[nodiscard]]
  bool createMeshAt(
    Entity hint,
    const Pose& pose,
    const Mesh& mesh,
    const Material& material
  );

  // Dense list of entities carrying Mesh.
  std::span<const Entity> meshes() const noexcept;

private:
  // MUST outlive m_registry because its on_destroy listener refers to this
  // store. Members are destroyed in reverse declaration order.
  SdfPrimitiveStore m_primitiveStore;

  entt::registry m_registry;
};

}  // namespace dunya::objectmodel
