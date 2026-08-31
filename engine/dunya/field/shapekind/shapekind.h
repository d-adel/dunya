#pragma once

#include <cstdint>

namespace dunya::field {

enum class ShapeKind : uint32_t {
  Sphere = DUNYA_SHAPE_SPHERE,
  Box = DUNYA_SHAPE_BOX,
  Plane = DUNYA_SHAPE_PLANE,
  Cylinder = DUNYA_SHAPE_CYLINDER
};

enum class CsgOperation : uint32_t {
  Union = DUNYA_CSG_UNION,
  SmoothUnion = DUNYA_CSG_SMOOTH_UNION,
  Intersection = DUNYA_CSG_INTERSECTION,
  Subtraction = DUNYA_CSG_SUBTRACTION,
  SmoothSubtraction = DUNYA_CSG_SMOOTH_SUBTRACTION
};

[[nodiscard]] constexpr ShapeKind shapeKindOf(uint32_t stored) noexcept {
  return static_cast<ShapeKind>(stored);
}

[[nodiscard]] constexpr CsgOperation operationOf(uint32_t stored) noexcept {
  return static_cast<CsgOperation>(stored);
}

[[nodiscard]] constexpr bool removesMaterial(CsgOperation operation) noexcept {
  switch (operation) {
    case CsgOperation::Subtraction:
    case CsgOperation::SmoothSubtraction:
      return true;
    case CsgOperation::Union:
    case CsgOperation::SmoothUnion:
    case CsgOperation::Intersection:
      return false;
  }

  return false;
}

[[nodiscard]] constexpr bool addsMaterial(CsgOperation operation) noexcept {
  switch (operation) {
    case CsgOperation::Union:
    case CsgOperation::SmoothUnion:
      return true;
    case CsgOperation::Intersection:
    case CsgOperation::Subtraction:
    case CsgOperation::SmoothSubtraction:
      return false;
  }

  return false;
}

}
