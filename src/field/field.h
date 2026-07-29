#pragma once
#include "glm/glm.hpp"

struct FieldPushConstants {
  glm::mat4 inverseViewProj;
  glm::vec4 cameraPos;
};
