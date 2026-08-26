#pragma once

#include "objectmodel/objectregistry/objectregistry.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <variant>

struct AddFieldObjectCommand {
  ObjectId id = INVALID_OBJECT_ID;
  FieldObject object;
};

struct RemoveFieldObjectCommand {
  ObjectId id;

  std::optional<FieldObject> object;
  std::vector<dunya::field::Primitive> primitives;
};

struct AddPrimitiveCommand {
  ObjectId objectId;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct RemovePrimitiveCommand {
  ObjectId objectId;
  uint32_t primitiveIndex;
  dunya::field::Primitive primitive;
};

struct UpdatePrimitiveCommand {
  ObjectId objectId;
  uint32_t primitiveIndex;

  dunya::field::Primitive oldPrimitive;
  dunya::field::Primitive newPrimitive;
};

struct TransformFieldObjectCommand {
  ObjectId id;

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
