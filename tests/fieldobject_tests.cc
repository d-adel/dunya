#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include "field/field.h"
#include "field/raycast/raycast.h"
#include "objectmodel/fieldobject/fieldobject.h"

#include "tolerances.h"

#include <optional>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::Primitive;
using dunya::field::Ray;
using dunya::field::RayHit;

namespace {

Primitive makeSphere(const glm::vec3& centre, float radius, uint32_t material) {
  Primitive primitive{};
  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
  primitive.shapeConfig = glm::uvec4(0, material, 0, 0);

  return primitive;
}

// The crossing the click path and the shader both do before they march.
Ray toLocal(const FieldObject& object, const Ray& world) {
  const glm::mat4 inverseModel = glm::inverse(object.model());

  return Ray{
    glm::vec3(inverseModel * glm::vec4(world.origin, 1.0f)),
    glm::vec3(inverseModel * glm::vec4(world.direction, 0.0f))
  };
}

}  // namespace

TEST_CASE("a pose puts an off-centre primitive where the turn says",
          "[fieldobject]") {
  // A quarter turn about Y swings local +X onto -Z, so the sphere authored at
  // local (2, 0, 0) has to be found at world (3, 0, -2) and nowhere else.
  FieldObject object{};
  object.position = glm::vec3(3.0f, 0.0f, 0.0f);
  object.rotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(2.0f, 0.0f, 0.0f), 1.0f, 3)
  };

  const Ray at{glm::vec3(3.0f, 0.0f, -6.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

  const std::optional<RayHit> hit =
    dunya::field::raymarch(primitives, toLocal(object, at));

  REQUIRE(hit.has_value());
  REQUIRE(hit->material == 3);

  const glm::vec3 world =
    glm::vec3(object.model() * glm::vec4(hit->position, 1.0f));

  REQUIRE_THAT(world.x, WithinAbs(3.0f, MARCH_TOLERANCE));
  REQUIRE_THAT(world.y, WithinAbs(0.0f, MARCH_TOLERANCE));
  REQUIRE_THAT(world.z, WithinAbs(-3.0f, MARCH_TOLERANCE));

  // Where an identity pose would have left the sphere. Rotating before
  // translating would put it at world (0, 0, -5), which this ray also misses.
  const Ray beside{glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f)};

  REQUIRE_FALSE(
    dunya::field::raymarch(primitives, toLocal(object, beside)).has_value()
  );
}

TEST_CASE("a point crossed into a pose and back comes home", "[fieldobject]") {
  // The march runs in local space and the shading runs in world space, so the
  // two crossings have to invert each other on an off-axis turn, not only at
  // identity.
  FieldObject object{};
  object.position = glm::vec3(-2.0f, 0.7f, 4.0f);
  object.rotation = glm::angleAxis(
    glm::radians(37.0f),
    glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f))
  );

  const glm::mat4 model = object.model();
  const glm::mat4 inverseModel = glm::inverse(model);

  for (const glm::vec3 point : {
         glm::vec3(0.0f),
         glm::vec3(1.0f, -2.0f, 3.0f),
         glm::vec3(-5.0f, 0.25f, 0.0f)
       }) {
    const glm::vec3 local = glm::vec3(inverseModel * glm::vec4(point, 1.0f));
    const glm::vec3 home = glm::vec3(model * glm::vec4(local, 1.0f));

    REQUIRE_THAT(home.x, WithinAbs(point.x, ANALYTIC_TOLERANCE));
    REQUIRE_THAT(home.y, WithinAbs(point.y, ANALYTIC_TOLERANCE));
    REQUIRE_THAT(home.z, WithinAbs(point.z, ANALYTIC_TOLERANCE));
  }
}

TEST_CASE("an accumulated rotation stays rigid", "[fieldobject]") {
  // A spinning object multiplies quaternions once a frame forever. The march
  // never renormalises its local direction, so a scale creeping in here would
  // break the distance bounds silently.
  FieldObject object{};

  for (int step = 0; step != 100000; ++step) {
    object.rotate(
      glm::radians(90.0f),
      glm::vec3(1.0f, 1.0f, 0.0f),
      1.0f / 60.0f
    );
  }

  REQUIRE_THAT(
    glm::length(object.rotation),
    WithinAbs(1.0f, ANALYTIC_TOLERANCE)
  );

  const glm::vec3 probe(1.0f, -2.0f, 0.5f);
  const glm::vec3 turned =
    glm::vec3(object.model() * glm::vec4(probe, 0.0f));

  REQUIRE_THAT(
    glm::length(turned),
    WithinAbs(glm::length(probe), ANALYTIC_TOLERANCE)
  );
}
