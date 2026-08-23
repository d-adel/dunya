#pragma once

#include "field/field.h"
#include "material/material.h"
#include "mesh/mesh.h"
#include "sampler/sampler.h"
#include "texture/texture.h"
#include "frame/frame.h"
#include "fieldobject/fieldobject.h"

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  Scene(const Context& context);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  void augmentFrameContext(Frame& frameContext);

  // Appends a sphere at the hit point. Returns false when the array is full,
  // which is a refusal rather than a corruption: the GPU buffer is a fixed
  // capacity and silently wrapping or dropping would be the worse answer.
  //
  // blend is the radius over which a smooth operation rounds, and is read only
  // by the smooth ops. It is a parameter rather than a policy chosen here
  // because the caller is what knows whether it is editing or benchmarking.
  bool addPrimitive(
    size_t objectIndex,
    const glm::vec3& centre,
    float radius,
    float blend,
    uint32_t material,
    uint32_t operation
  );

  bool addFieldObject();
  void setVolumeIndex(size_t objectIndex, uint32_t volumeIndex);
  void setDirty(size_t objectIndex, bool value);

  const std::vector<dunya::field::Primitive>& primitives() const noexcept;
  const std::vector<Material>& materials() const noexcept;
  const std::vector<Texture>& textures() const noexcept;
  const std::vector<Sampler>& samplers() const noexcept;
  const std::vector<FieldObject>& fieldObjects() const noexcept;

private:
  static std::vector<dunya::field::Primitive> createPrimitives();
  static std::vector<Material> createMaterials();
  static std::vector<Texture> createTextures(const Device& device);
  static std::vector<Sampler> createSamplers(const Device& device);

  std::vector<Material> m_materials;
  std::vector<Sampler> m_samplers;
  std::vector<Texture> m_textures;
  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
  std::vector<FieldObject> m_fieldObjects;
  std::vector<FieldObjectShared> m_sharedFieldObjects;
};
