#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace dunya::editor {

struct AddFieldObjectCommand {
  dunya::objectmodel::Entity entity = dunya::objectmodel::INVALID_ENTITY;
  dunya::objectmodel::FieldObject object;
};

struct RemoveFieldObjectCommand {
  dunya::objectmodel::Entity entity;
  std::optional<dunya::objectmodel::FieldObject> object;
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

struct TransformFieldObjectCommand {
  dunya::objectmodel::Entity entity;

  glm::vec3 oldPosition;
  glm::quat oldRotation;

  glm::vec3 newPosition;
  glm::quat newRotation;
};

using Command = std::variant<
  AddFieldObjectCommand,
  RemoveFieldObjectCommand,
  TransformFieldObjectCommand,
  AddPrimitiveCommand,
  RemovePrimitiveCommand,
  UpdatePrimitiveCommand>;

}  // namespace dunya::editor
