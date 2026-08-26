#pragma once

#include "field/field.h"
#include "objectmodel/material/material.h"
#include "mesh/mesh.h"
#include "gpu/sampler/sampler.h"
#include "gpu/texture/texture.h"
#include "frame/frame.h"
#include "objectmodel/fieldobject/fieldobject.h"
#include "objectmodel/objectregistry/objectregistry.h"
#include "objectmodel/commandhistory/commandhistory.h"

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

  bool addPrimitive(
    ObjectId objectIndex,
    const glm::vec3& centre,
    float radius,
    float blend,
    uint32_t material,
    uint32_t operation
  );

  ObjectId addFieldObject(glm::vec3 position);
  void setVolumeIndex(ObjectId objectIndex, uint32_t volumeIndex);
  void setDirty(ObjectId objectIndex, bool value);

  const ObjectRegistry& registry() const;
  ObjectRegistry& registry();

  const std::vector<Material>& materials() const noexcept;
  const std::vector<Texture>& textures() const noexcept;
  const std::vector<Sampler>& samplers() const noexcept;

  void undo();
  void redo();

private:
  static std::vector<Material> createMaterials();
  static std::vector<Texture> createTextures(const Device& device);
  static std::vector<Sampler> createSamplers(const Device& device);

  void addInitialPrimitives(ObjectId objectId);

  std::vector<Material> m_materials;
  std::vector<Sampler> m_samplers;
  std::vector<Texture> m_textures;
  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
  ObjectRegistry m_objectRegistry;
  CommandHistory m_commandHistory;
};
