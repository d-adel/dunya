#pragma once

#include "config/config.h"
#include "frameglobals/frameglobals.h"
#include "mesh/mesh.h"
#include "pipeline/pipeline.h"
#include "field/field.h"
#include "fieldobject/fieldobject.h"

#include <glm/glm.hpp>
#include <span>

struct DrawItem {
  uint32_t meshIndex;
  uint32_t materialIndex;
  glm::mat4 model;
};

struct Frame {
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 proj = glm::mat4(1.0f);
  glm::vec4 cameraPos = glm::vec4(1.0f);
  std::span<const DrawItem> drawItems = {};
  std::span<const Mesh> meshes = {};

  std::span<const ObjectId> fieldObjectIds = {};
  std::span<const dunya::field::Primitive> primitives = {};

  PipelineType mode = PipelineType::Both;

  uint32_t fieldRepresentation = FIELD_SAMPLED;

  // Authoritative here rather than in the shader, so a slider and the CPU's own
  // marching read the same numbers. Defaults come from CMake.
  MarchParams march{
    DUNYA_MARCH_EPSILON,
    DUNYA_MARCH_MAX_DISTANCE,
    DUNYA_MARCH_OMEGA,
    DUNYA_GRID_STEP_SAFETY,
    DUNYA_GRADIENT_EPSILON,
    DUNYA_SHADOW_MAX_DISTANCE,
    DUNYA_SHADOW_SHARPNESS,
    DUNYA_MARCH_MAX_ITERATIONS
  };
};
