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

// A primitive can be skipped only when it provably cannot change the
// accumulator. The conditions differ per operation and getting them wrong is
// silent, so they are stated once here and mirrored exactly in the shader.
bool skippable(
  const Primitive& primitive,
  const glm::vec3& point,
  float accumulated
) {
  if (primitive.bounds.w <= 0.0f) {
    return false;
  }

  const float bound =
    glm::length(point - glm::vec3(primitive.bounds)) - primitive.bounds.w;

  switch (primitive.shapeConfig.z) {
    // Union and smooth union lose to a nearer accumulator. The blend radius is
    // folded into the stored radius, so a smooth union that could still pull
    // the surface toward itself never satisfies this.
    case 0u:
    case 1u:
      return bound > accumulated;

    // Subtraction is max(acc, -cur), which only bites when the cutter reaches
    // within the accumulated distance. The smooth one bites k sooner, and that
    // k is already folded into the stored radius by updateBounds, so the two
    // share this test rather than needing a second one.
    case 3u:
    case 4u:
      return bound >= -accumulated;

    // Intersection takes a max, so an arbitrarily distant primitive still
    // dominates and can never be skipped on distance.
    default:
      return false;
  }
}

}  // namespace

void updateBounds(Primitive& primitive) {
  const glm::mat4 model = glm::inverse(primitive.inverseModel);
  const glm::vec3 centre = glm::vec3(model[3]);

  float radius = 0.0f;

  switch (primitive.shapeConfig.x) {
    case 0u:
      radius = primitive.shape.x;
      break;

    case 1u:
      radius = glm::length(glm::vec3(primitive.shape));
      break;

    // A plane is unbounded, and an unknown shape has no bound we can trust.
    default:
      radius = 0.0f;
      break;
  }

  // A blend reaches further than the shape does, in both directions: a smooth
  // union pulls the surface toward a neighbour that has not touched it yet, and
  // a smooth subtraction starts rounding before the cutter arrives. Either way
  // the primitive matters k earlier than its own radius says, so the bound has
  // to carry k or skippable() will cull one that still had work to do.
  if (
    radius > 0.0f
    && (primitive.shapeConfig.z == 1u || primitive.shapeConfig.z == 4u)
  ) {
    radius += primitive.shape.w;
  }

  primitive.bounds = glm::vec4(centre, radius);
}

std::optional<Aabb> boundedExtent(std::span<const Primitive> primitives) {
  bool any = false;
  Aabb extent{glm::vec3(0.0f), glm::vec3(0.0f)};

  for (const Primitive& primitive : primitives) {
    if (primitive.bounds.w <= 0.0f) {
      continue;
    }

    const glm::vec3 centre = glm::vec3(primitive.bounds);
    const glm::vec3 radius = glm::vec3(primitive.bounds.w);

    if (!any) {
      extent = {centre - radius, centre + radius};
      any = true;
      continue;
    }

    extent.minimum = glm::min(extent.minimum, centre - radius);
    extent.maximum = glm::max(extent.maximum, centre + radius);
  }

  if (!any) {
    return std::nullopt;
  }

  return extent;
}

AnalyticSample sample(
  std::span<const Primitive> primitives,
  const glm::vec3& point
) {
  AnalyticSample accumulated{FAR_DISTANCE, 0};

  for (const Primitive& primitive : primitives) {
    if (skippable(primitive, point, accumulated.distance)) {
      continue;
    }

    const AnalyticSample current{
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

      // Smooth subtraction, which is smooth max applied to the negated cutter:
      // smax(a, b, k) = -smin(-a, -b, k), and b is -cur, so the inner negation
      // cancels. One blend primitive, used both ways.
      //
      // Where a hard subtraction leaves a crease at every intersection circle,
      // this rounds it - and a crease is what shading shows, because a normal
      // is a derivative (idiom 31). It costs a little conservatism: smax sits
      // up to k/4 above the hard max, so unlike smin it is not automatically a
      // safe under-estimate for sphere tracing.
      case 4u:
        accumulated = {
          -smoothMin(
            -accumulated.distance,
            current.distance,
            primitive.shape.w
          ),
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

float distance(AnalyticFieldView field, const glm::vec3& point) {
  return sample(field.primitives, point).distance;
}

uint32_t material(AnalyticFieldView field, const glm::vec3& point) {
  return sample(field.primitives, point).material;
}

glm::vec3 gradient(
  AnalyticFieldView field,
  const glm::vec3& point,
  float epsilon
) {
  return gradient(field.primitives, point, epsilon);
}

// The direction is unused: the bound is the same in every direction, which is
// what sphere tracing is.
float stepBound(
  AnalyticFieldView field,
  const glm::vec3& point,
  const glm::vec3&
) {
  return std::abs(distance(field, point));
}

}  // namespace dunya::field
