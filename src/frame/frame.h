#pragma once

#include "mesh/mesh.h"
#include "pipeline/pipeline.h"
#include "field/field.h"

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
  std::span<const dunya::field::Primitive> primitives = {};
  PipelineType mode = PipelineType::Both;
};
