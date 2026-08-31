#pragma once

#include <dunya/field/sampledsdf/sampledsdf.h>

#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/mesh/mesh.h>
#include <dunya/objectmodel/component/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/deformed/deformed.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/trait/authored/authored.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/dynamiccomponents/dynamiccomponents.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>
#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>
#include <dunya/objectmodel/component/sharedfield/sharedfield.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <type_traits>
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

  const entt::registry& registry() const noexcept;

  [[nodiscard]] DynamicComponents& dynamic() noexcept;
  [[nodiscard]] const DynamicComponents& dynamic() const noexcept;

  void clear();

  Entity createAuthored();

  [[nodiscard]]
  bool createAuthoredAt(Entity hint);

  template<Authored T>
  void emplaceAuthored(Entity entity, const T& value) {
    if constexpr (std::is_empty_v<T>) {
      m_registry.emplace_or_replace<T>(entity);
    } else {
      m_registry.emplace_or_replace<T>(entity, value);
    }
  }

  Entity createSdfGrid(const Pose& pose, const SdfGrid& grid);

  [[nodiscard]]
  bool createSdfGridAt(Entity hint, const Pose& pose, const SdfGrid& grid);

  [[nodiscard]]
  bool destroy(Entity entity);

  [[nodiscard]] std::span<const Entity> fields() const noexcept;

  [[nodiscard]] std::span<const Entity> sdfGrids() const noexcept;

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

  void addStaticBody(Entity entity);

  std::span<const dunya::field::Primitive> primitives(Entity entity) const;

  uint32_t primitiveCount(Entity entity) const;

  std::span<const dunya::field::Primitive> pool() const noexcept;

  template<SelfContained T, typename Fn>
  void patch(Entity entity, Fn&& fn) {
    m_registry.patch<T>(entity, std::forward<Fn>(fn));
  }

  template<typename Fn>
  void patchSampledSdf(Entity entity, Fn&& fn) {
    if (!m_registry.all_of<Deformable>(entity)) {
      throw std::runtime_error(
        "Only a deformable's lattice may be written in place"
      );
    }

    SharedSdf& held = m_registry.get<SharedSdf>(entity);

    if (held.field.use_count() > 1) {
      held.field = std::make_shared<dunya::field::SampledSdf>(*held.field);
    }

    fn(*held.field);

    m_registry.emplace_or_replace<Deformed>(entity);
  }

  template<SelfContained T>
  void emplaceOrReplace(Entity entity, const T& value) {
    m_registry.emplace_or_replace<T>(entity, value);
  }

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

  void markSdfDirty(Entity entity, const dunya::field::SampleBox& box);

  [[nodiscard]] std::span<const std::pair<Entity, dunya::field::SampleBox>>
  sdfDirty() const noexcept;

  void clearSdfDirty() noexcept;

  void setBakedVolume(Entity entity, uint32_t index);

  void setRigidBody(Entity entity, uint32_t index);

  void setSampledSdf(Entity entity, dunya::field::SampledSdf field);

  void shareSampledSdf(Entity donor, Entity taker);

  void adoptSampledSdf(Entity entity, const SharedSdf& held);

  [[nodiscard]]
  const dunya::field::SampledSdf* sampledSdf(Entity entity) const;

  [[nodiscard]]
  bool hasSampledSdf(Entity entity) const noexcept;

  [[nodiscard]]
  long sampledSdfUsers(Entity entity) const noexcept;

  void clearBakedVolume(Entity entity);

  bool needsBake(Entity entity) const noexcept;
  void markBaked(Entity entity);

  bool needsResample(Entity entity) const noexcept;

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

  std::span<const Entity> meshes() const noexcept;

private:
  SdfPrimitiveStore m_primitiveStore;

  DynamicComponents m_dynamic;

  std::vector<std::pair<Entity, dunya::field::SampleBox>> m_sdfDirty;

  entt::registry m_registry;
};

}
