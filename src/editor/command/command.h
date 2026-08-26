#pragma once

#include "objectmodel/objectregistry/objectregistry.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <variant>

namespace dunya::editor {

struct AddFieldObjectCommand {
  dunya::core::ObjectId id = dunya::core::INVALID_OBJECT_ID;
  dunya::objectmodel::FieldObject object;
};

struct RemoveFieldObjectCommand {
  dunya::core::ObjectId id;

  std::optional<dunya::objectmodel::FieldObject> object;
  std::vector<dunya::field::Primitive> primitives;
};

struct AddPrimitiveCommand {
  dunya::core::ObjectId objectId;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct RemovePrimitiveCommand {
  dunya::core::ObjectId objectId;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct UpdatePrimitiveCommand {
  dunya::core::ObjectId objectId;
  uint32_t primitiveIndex;

  dunya::field::Primitive oldPrimitive;
  dunya::field::Primitive newPrimitive;
};

struct TransformFieldObjectCommand {
  dunya::core::ObjectId id;

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
