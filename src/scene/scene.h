#pragma once

#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/renderer/meshrecord/meshrecord.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/world/world.h>

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  Scene(const dunya::gpu::Context& context, dunya::objectmodel::World& world);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  // Takes the world to read rather than using the one it filled: with Play
  // there are two, and the frame must not mix records from both.
  void augmentFrameContext(
    dunya::renderer::Frame& frameContext,
    const dunya::objectmodel::World& world
  );

  const std::vector<dunya::renderer::MaterialRecord>&
  materials() const noexcept;
  const std::vector<dunya::gpu::Texture>& textures() const noexcept;
  const std::vector<dunya::gpu::Sampler>& samplers() const noexcept;

  // What a shot spawns. Described rather than owned: a ball is created in the
  // runtime world when the key is pressed, and dies with it.
  struct Projectile {
    dunya::objectmodel::SdfGrid grid;
    dunya::field::Primitive shape;
    float speed = 0.0f;

    // Where a ball leaves from, and what it is thrown at. Aimed rather than
    // fired along the view, because a camera looking down at the scene would
    // send every shot into the floor.
    float height = 0.0f;
    glm::vec3 aimAt{0.0f};

    // Overrides what the shape derives from the volume it describes, so the
    // same ball can be a marble or a wrecking ball.
    float mass = 100.0f;
  };

  [[nodiscard]]
  Projectile projectile() const;

  // Baked once here so a shot does not bake. Every ball is the same sphere in
  // its own local space, so one field describes all of them.
  [[nodiscard]]
  const dunya::field::SampledField& projectileField() const noexcept;

  // Points the camera at the layout above.
  void frame(dunya::objectmodel::Camera& camera) const;

private:
  static std::vector<dunya::renderer::MaterialRecord> createMaterials();
  static std::vector<dunya::gpu::Texture> createTextures(
    const dunya::gpu::Device& device
  );
  static std::vector<dunya::gpu::Sampler> createSamplers(
    const dunya::gpu::Device& device
  );

  // Every primitive in this scene goes through here, because a refusal is a
  // full arena and that is worth saying which object it happened to.
  void addPrimitive(
    dunya::objectmodel::Entity entity,
    const dunya::field::Primitive& primitive,
    const char* what
  );

  std::vector<dunya::renderer::MaterialRecord> m_materials;
  std::vector<dunya::gpu::Sampler> m_samplers;
  std::vector<dunya::gpu::Texture> m_textures;
  std::vector<dunya::renderer::MeshBuffers> m_meshes;

  // Rebuilt every frame; a member so the span handed to Frame stays alive.
  std::vector<dunya::renderer::MeshRecord> m_meshRecords;

  // The authored world, owned by Application. Scene fills it and reads it;
  // it does not own it, because Play needs a second one beside it.
  dunya::objectmodel::World& m_world;

  dunya::field::SampledField m_projectileField;
};
