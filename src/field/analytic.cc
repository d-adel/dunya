#include "analytic.ih"

namespace dunya::field {

namespace {

constexpr float FAR_DISTANCE = 1e9f;

float smoothMin(float a, float b, float k) {
  const float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);

  return (b * (1.0f - h) + a * h) - k * h * (1.0f - h);
}

float boxDistance(const glm::vec3& point, const glm::vec3& halfExtents) {
  const glm::vec3 q = glm::abs(point) - halfExtents;

  const float outside = glm::length(glm::max(q, glm::vec3(0.0f)));
  const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);

  return outside + inside;
}

float primitiveDistance(const Primitive& primitive, const glm::vec3& point) {
  const glm::vec3 local =
    glm::vec3(primitive.inverseModel * glm::vec4(point, 1.0f));

  switch (primitive.shapeConfig.x) {
    case 0u:
      return glm::length(local) - primitive.shape.x;
    case 1u:
      return boxDistance(local, glm::vec3(primitive.shape));
    case 2u:
      return local.y;
    default:
      return FAR_DISTANCE;
  }
}

}  // namespace

FieldSample sample(
  std::span<const Primitive> primitives,
  const glm::vec3& point
) {
  FieldSample accumulated{FAR_DISTANCE, 0};

  for (const Primitive& primitive : primitives) {
    const FieldSample current{
      primitiveDistance(primitive, point),
      primitive.shapeConfig.y
    };

    switch (primitive.shapeConfig.z) {
      case 1u:
        accumulated = {
          smoothMin(accumulated.distance, current.distance, primitive.shape.w),
          accumulated.distance < current.distance ? accumulated.material
                                                  : current.material
        };
        break;

      case 2u:
        accumulated =
          accumulated.distance > current.distance ? accumulated : current;
        break;

      case 3u:
        accumulated = {
          std::max(accumulated.distance, -current.distance),
          accumulated.material
        };
        break;

      default:
        accumulated =
          accumulated.distance < current.distance ? accumulated : current;
        break;
    }
  }

  return accumulated;
}

glm::vec3 gradient(
  std::span<const Primitive> primitives,
  const glm::vec3& point,
  float epsilon
) {
  const glm::vec3 offsetX(epsilon, 0.0f, 0.0f);
  const glm::vec3 offsetY(0.0f, epsilon, 0.0f);
  const glm::vec3 offsetZ(0.0f, 0.0f, epsilon);

  const float scale = 1.0f / (2.0f * epsilon);

  return glm::vec3(
    (sample(primitives, point + offsetX).distance
     - sample(primitives, point - offsetX).distance)
      * scale,
    (sample(primitives, point + offsetY).distance
     - sample(primitives, point - offsetY).distance)
      * scale,
    (sample(primitives, point + offsetZ).distance
     - sample(primitives, point - offsetZ).distance)
      * scale
  );
}

glm::vec3 normal(
  std::span<const Primitive> primitives,
  const glm::vec3& point,
  float epsilon
) {
  return glm::normalize(gradient(primitives, point, epsilon));
}

}  // namespace dunya::field
