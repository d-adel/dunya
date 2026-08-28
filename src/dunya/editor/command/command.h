#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/pose/pose.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace dunya::editor {

struct CreateFieldCommand {
  dunya::objectmodel::Entity entity = dunya::objectmodel::INVALID_ENTITY;
  dunya::objectmodel::Pose pose;
  dunya::objectmodel::SdfGrid grid;
};

struct DestroyFieldCommand {
  dunya::objectmodel::Entity entity;
  std::optional<dunya::objectmodel::Pose> pose;
  std::optional<dunya::objectmodel::SdfGrid> grid;
  std::vector<dunya::field::Primitive> primitives;
};

struct AddPrimitiveCommand {
  dunya::objectmodel::Entity entity;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct RemovePrimitiveCommand {
  dunya::objectmodel::Entity entity;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct UpdatePrimitiveCommand {
  dunya::objectmodel::Entity entity;
  uint32_t primitiveIndex;

  dunya::field::Primitive oldPrimitive;
  dunya::field::Primitive newPrimitive;
};

struct TransformFieldCommand {
  dunya::objectmodel::Entity entity;

  glm::vec3 oldPosition;
  glm::quat oldRotation;

  glm::vec3 newPosition;
  glm::quat newRotation;
};

using Command = std::variant<
  CreateFieldCommand,
  DestroyFieldCommand,
  TransformFieldCommand,
  AddPrimitiveCommand,
  RemovePrimitiveCommand,
  UpdatePrimitiveCommand>;

}  // namespace dunya::editor
