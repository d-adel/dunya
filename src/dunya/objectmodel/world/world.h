#pragma once

#include <dunya/objectmodel/material/material.h>
#include <dunya/objectmodel/mesh/mesh.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>
#include <dunya/objectmodel/selfcontained/selfcontained.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

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

  // Owns a slot in the renderer's volume pool, so it is not self-contained
  // and keeps a method of its own.
  void setBakedVolume(Entity entity, uint32_t index);

  // Change tracking, not a flag: the queue is entt::reactive storage filled
  // from the registry's own signals, so a new mutation path is covered without
  // anyone remembering to mark it.
  bool needsBake(Entity entity) const noexcept;
  void markBaked(Entity entity);

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
