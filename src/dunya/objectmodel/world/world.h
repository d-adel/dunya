#pragma once

#include <dunya/field/sampled/sampled.h>

#include <dunya/objectmodel/material/material.h>
#include <dunya/objectmodel/mesh/mesh.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/deformable/deformable.h>
#include <dunya/objectmodel/deformed/deformed.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>
#include <dunya/objectmodel/selfcontained/selfcontained.h>
#include <dunya/objectmodel/sharedfield/sharedfield.h>
#include <dunya/objectmodel/rigidbody/rigidbody.h>
#include <dunya/objectmodel/staticbody/staticbody.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <cstddef>
#include <stdexcept>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

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

  // The one way a lattice changes in place after its bake. Deliberately not
  // patch<T>: a lattice is derived from the primitives, which is why it
  // refuses SelfContained, and a dent is the operation that ends that
  // derivation. Only a Deformable may do it, so the tag is the permission
  // rather than a comment - and this stays a reference because a dent touches
  // a few dozen voxels of a 16 MiB grid, so copying it out is the cost the
  // whole milestone exists to avoid.
  //
  // Copy on write, and this is the write: a lattice several objects hold is
  // copied first, so a dent in one crate does not appear in every crate cut
  // from the same primitives. use_count answers it exactly, because a
  // collision shape borrows the address rather than holding a reference.
  template<typename Fn>
  void patchSampledField(Entity entity, Fn&& fn) {
    if (!m_registry.all_of<Deformable>(entity)) {
      throw std::runtime_error(
        "Only a deformable's lattice may be written in place"
      );
    }

    SharedField& held = m_registry.get<SharedField>(entity);

    if (held.field.use_count() > 1) {
      held.field = std::make_shared<dunya::field::SampledField>(*held.field);
    }

    fn(*held.field);

    // Recorded here rather than by the caller, for the reason the bake queue
    // gives: a mutation path that has to be remembered is one that will be
    // forgotten. This is the only way a lattice diverges from its primitives.
    m_registry.emplace_or_replace<Deformed>(entity);
  }

  // Write it whether or not it is there. EnTT's verb and EnTT's
  // precondition, which is none — for a component whose absence is a
  // meaningful state, and which therefore has nowhere to be created.
  template<SelfContained T>
  void emplaceOrReplace(Entity entity, const T& value) {
    m_registry.emplace_or_replace<T>(entity, value);
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

  // Hands one object's lattice to another instead of a copy of it. What makes
  // a thousand identical crates cost one lattice; the first dent on any of
  // them takes its own through patchSampledField.
  void shareSampledField(Entity donor, Entity taker);

  // The same, across worlds: instantiation hands the runtime the authored
  // lattice rather than 720 MB of copies, and the runtime's first dent takes
  // its own. The handle is the parameter because the donor is in another
  // registry and there is no entity here to name it by.
  void adoptSampledField(Entity entity, const SharedField& held);

  // Null when the object has none, which is the state before the first bake.
  [[nodiscard]]
  const dunya::field::SampledField* sampledField(Entity entity) const;

  [[nodiscard]]
  bool hasSampledField(Entity entity) const noexcept;

  // How many objects hold this one's lattice, counting itself. One means a
  // write needs no copy.
  [[nodiscard]]
  long sampledFieldUsers(Entity entity) const noexcept;

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
