#pragma once

#include <dunya/core/config/config.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/renderer/drawmode/drawmode.h>
#include <dunya/field/field.h>
#include <dunya/renderer/meshrecord/meshrecord.h>
#include <dunya/objectmodel/component/directionallight/directionallight.h>
#include <dunya/objectmodel/component/environment/environment.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>

#include <glm/glm.hpp>
#include <optional>
#include <span>

namespace dunya::renderer {

struct Frame {
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 proj = glm::mat4(1.0f);
  glm::vec4 cameraPos = glm::vec4(1.0f);
  std::span<const MeshRecord> meshRecords = {};
  std::span<const MeshBuffers> meshes = {};

  uint32_t sdfRecordCount = 0;
  std::span<const dunya::field::Primitive> primitives = {};

  DrawMode mode = DrawMode::Both;

  uint32_t fieldRepresentation = dunya::core::FIELD_SAMPLED;

  dunya::objectmodel::DirectionalLight light{};

  std::optional<dunya::objectmodel::Environment> environment{};

  MarchParams march{
    DUNYA_MARCH_EPSILON,
    DUNYA_MARCH_MAX_DISTANCE,
    DUNYA_MARCH_OMEGA,
    DUNYA_GRADIENT_EPSILON,
    DUNYA_SHADOW_MAX_DISTANCE,
    DUNYA_SHADOW_SHARPNESS,
    DUNYA_MARCH_MAX_ITERATIONS
  };
};

}
