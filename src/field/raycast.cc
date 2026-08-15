#include "raycast.ih"

namespace dunya::field {

Ray screenPointToRay(
  const glm::mat4& inverseViewProj,
  const glm::vec3& cameraPosition,
  const glm::vec2& ndc
) {
  glm::vec4 world = inverseViewProj * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
  world /= world.w;

  return {cameraPosition, glm::normalize(glm::vec3(world) - cameraPosition)};
}

std::optional<RayHit> raymarch(
  std::span<const Primitive> primitives,
  const Ray& ray,
  const MarchSettings& settings
) {
  // Enhanced sphere tracing: step by omega times the distance, and when the
  // current unbounding sphere no longer reaches back to the previous one the
  // step may have jumped a surface, so undo part of it and continue unrelaxed.
  // Mirrored exactly in field-shader.frag; a CPU march that disagreed with the
  // shader would carve somewhere other than where the cursor pointed.
  float omega = settings.omega;
  float travelled = 0.0f;
  float previousRadius = 0.0f;
  float stepLength = 0.0f;
  float functionSign = 1.0f;

  for (int i = 0; i < settings.maxIterations; ++i) {
    const glm::vec3 point = ray.origin + ray.direction * travelled;
    const FieldSample current = sample(primitives, point);

    // Marching from inside solid geometry means walking the distance back up
    // to zero, so the sign is taken once from where the ray starts.
    if (i == 0) {
      functionSign = current.distance < 0.0f ? -1.0f : 1.0f;
    }

    const float signedRadius = functionSign * current.distance;
    const float radius = std::abs(signedRadius);

    const bool relaxationFailed =
      omega > 1.0f && (radius + previousRadius) < stepLength;

    if (relaxationFailed) {
      stepLength -= omega * stepLength;
      omega = 1.0f;
    } else {
      stepLength = signedRadius * omega;
    }

    previousRadius = radius;

    if (!relaxationFailed && radius <= settings.epsilon) {
      return RayHit{point, travelled, current.material};
    }

    travelled += stepLength;

    if (travelled > settings.maxDistance) {
      break;
    }
  }

  return std::nullopt;
}

}  // namespace dunya::field
