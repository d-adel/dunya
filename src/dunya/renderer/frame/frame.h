#pragma once

#include <dunya/core/config/config.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/mesh/mesh.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/drawitem/drawitem.h>
#include <dunya/objectmodel/fieldgrid/fieldgrid.h>

#include <glm/glm.hpp>
#include <span>

namespace dunya::renderer {

struct Frame {
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 proj = glm::mat4(1.0f);
  glm::vec4 cameraPos = glm::vec4(1.0f);
  std::span<const dunya::objectmodel::DrawItem> drawItems = {};
  std::span<const Mesh> meshes = {};

  uint32_t fieldRecordCount = 0;
  std::span<const dunya::field::Primitive> primitives = {};

  dunya::gpu::PipelineType mode = dunya::gpu::PipelineType::Both;

  uint32_t fieldRepresentation = dunya::core::FIELD_SAMPLED;

  // Authoritative here rather than in the shader, so a slider and the CPU's own
  // marching read the same numbers. Defaults come from CMake.
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

}  // namespace dunya::renderer
