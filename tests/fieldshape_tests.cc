#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/physics/fieldshape/fieldshape.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <vector>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using dunya::field::Primitive;
using dunya::field::SampledField;
using dunya::physics::FieldShape;
using dunya::physics::JoltLibrary;

namespace {

constexpr uint32_t SPHERE = 0;

constexpr float DENSITY = 1000.0f;

Primitive makeSphere(
  const glm::vec3& centre,
  float radius,
  uint32_t operation = dunya::core::FIELD_OP_UNION
) {
  Primitive primitive{};
  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
  primitive.shapeConfig = glm::uvec4(SPHERE, 1u, operation, 0u);
  dunya::field::updateBounds(primitive);

  return primitive;
}

// One sphere in a box that clears it by half a radius, so the grid holds
// outside as well as inside.
SampledField bakeSphere(float radius, uint32_t resolution) {
  const float reach = radius * 1.5f;

  return dunya::field::bake(
    std::vector<Primitive>{makeSphere(glm::vec3(0.0f), radius)},
    glm::vec3(-reach),
    glm::vec3(reach),
    glm::uvec3(resolution)
  );
}

float sphereMass(float radius) {
  return DENSITY * 4.0f / 3.0f * glm::pi<float>() * radius * radius * radius;
}

}  // namespace

TEST_CASE("the local bounds are the grid, not the solid", "[fieldshape]") {
  // Jolt culls on this box, so it has to be the extent the field can answer
  // for. One voxel too large is a phantom contact at the far faces; one too
  // small drops contacts near them.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 17u);
  const FieldShape shape(field);

  const JPH::AABox bounds = shape.GetLocalBounds();

  const glm::vec3 far =
    field.origin
    + field.voxelSize * glm::vec3(field.resolution - glm::uvec3(1u));

  REQUIRE_THAT(bounds.mMin.GetX(), WithinAbs(field.origin.x, 1e-6f));
  REQUIRE_THAT(bounds.mMin.GetY(), WithinAbs(field.origin.y, 1e-6f));
  REQUIRE_THAT(bounds.mMin.GetZ(), WithinAbs(field.origin.z, 1e-6f));
  REQUIRE_THAT(bounds.mMax.GetX(), WithinAbs(far.x, 1e-6f));
  REQUIRE_THAT(bounds.mMax.GetY(), WithinAbs(far.y, 1e-6f));
  REQUIRE_THAT(bounds.mMax.GetZ(), WithinAbs(far.z, 1e-6f));
}

TEST_CASE("the inner radius is the deepest point inside", "[fieldshape]") {
  // Jolt reads it as the distance a body may travel before a cast is worth
  // doing, so it has to be the inscribed sphere and never zero.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 33u);
  const FieldShape shape(field);

  REQUIRE_THAT(shape.GetInnerRadius(), WithinAbs(1.0f, field.voxelSize.x));
}

TEST_CASE("an empty grid still reports a usable inner radius", "[fieldshape]") {
  // Nothing solid means nothing negative, and a zero here trips Jolt's own
  // assert rather than producing a slow body.
  JoltLibrary library;

  const SampledField field = dunya::field::bake(
    std::vector<Primitive>{makeSphere(glm::vec3(10.0f), 0.5f)},
    glm::vec3(-1.0f),
    glm::vec3(1.0f),
    glm::uvec3(17u)
  );

  const FieldShape shape(field);

  REQUIRE(shape.GetInnerRadius() > 0.0f);
}

TEST_CASE("the mass is the solid's, not the grid box's", "[fieldshape]") {
  // The box is 27 times the sphere here, so integrating the wrong region is
  // not a near miss.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u);
  const FieldShape shape(field);

  REQUIRE_THAT(
    shape.GetMassProperties().mMass,
    WithinRel(sphereMass(1.0f), 0.05f)
  );
}

TEST_CASE("the inertia is the solid's, about its own axes", "[fieldshape]") {
  // A sphere's tensor is isotropic, which makes both halves visible at once:
  // the diagonal names the distribution, the off-diagonal has to vanish.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u);
  const FieldShape shape(field);

  const JPH::MassProperties properties = shape.GetMassProperties();
  const float expected = 0.4f * properties.mMass;

  for (int axis = 0; axis != 3; ++axis) {
    REQUIRE_THAT(properties.mInertia(axis, axis), WithinRel(expected, 0.05f));
  }

  REQUIRE_THAT(properties.mInertia(0, 1), WithinAbs(0.0f, expected * 0.01f));
  REQUIRE_THAT(properties.mInertia(0, 2), WithinAbs(0.0f, expected * 0.01f));
  REQUIRE_THAT(properties.mInertia(1, 2), WithinAbs(0.0f, expected * 0.01f));
}

TEST_CASE("carving the solid takes mass with it", "[fieldshape]") {
  // The property denting depends on: mass follows the geometry, so a body
  // that loses material loses weight without anyone maintaining a number.
  JoltLibrary library;

  const SampledField whole = bakeSphere(1.0f, 65u);

  const SampledField carved = dunya::field::bake(
    std::vector<Primitive>{
      makeSphere(glm::vec3(0.0f), 1.0f),
      makeSphere(
        glm::vec3(0.9f, 0.0f, 0.0f),
        0.5f,
        dunya::core::FIELD_OP_SUBTRACTION
      )
    },
    glm::vec3(-1.5f),
    glm::vec3(1.5f),
    glm::uvec3(65u)
  );

  const float lost = FieldShape(whole).GetMassProperties().mMass
                     - FieldShape(carved).GetMassProperties().mMass;

  // The bite is the part of the small sphere lying inside the large one, which
  // is most of it but not all, so this brackets rather than equates.
  REQUIRE(lost > sphereMass(0.5f) * 0.5f);
  REQUIRE(lost < sphereMass(0.5f));
}

TEST_CASE("every brick has a sub shape id of its own", "[fieldshape]") {
  // Two bricks sharing an id collide in Jolt's manifold cache, which is a
  // warm start reading last frame's contact for a different part of the body.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 33u);
  const FieldShape shape(field);

  const glm::uvec3 bricks = dunya::field::brickCounts(field);
  const uint32_t count = bricks.x * bricks.y * bricks.z;

  REQUIRE(count > 1u);
  REQUIRE((uint64_t{1} << shape.GetSubShapeIDBitsRecursive()) >= count);
}

TEST_CASE("the surface normal points out of the solid", "[fieldshape]") {
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 33u);
  const FieldShape shape(field);

  const JPH::Vec3 normal =
    shape.GetSurfaceNormal(JPH::SubShapeID(), JPH::Vec3(1.0f, 0.0f, 0.0f));

  REQUIRE_THAT(normal.GetX(), WithinAbs(1.0f, 0.05f));
  REQUIRE_THAT(normal.GetY(), WithinAbs(0.0f, 0.05f));
  REQUIRE_THAT(normal.GetZ(), WithinAbs(0.0f, 0.05f));
}

TEST_CASE("a point inside collides and one outside does not", "[fieldshape]") {
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 33u);
  const FieldShape shape(field);

  // The collector reads a body id off its context, which a query through a
  // physics system supplies and a direct call has to stand in for.
  const JPH::TransformedShape context{};

  JPH::AllHitCollisionCollector<JPH::CollidePointCollector> inside;
  inside.SetContext(&context);

  shape.CollidePoint(
    JPH::Vec3(0.0f, 0.0f, 0.0f),
    JPH::SubShapeIDCreator(),
    inside,
    JPH::ShapeFilter()
  );

  JPH::AllHitCollisionCollector<JPH::CollidePointCollector> outside;
  outside.SetContext(&context);

  shape.CollidePoint(
    JPH::Vec3(1.4f, 0.0f, 0.0f),
    JPH::SubShapeIDCreator(),
    outside,
    JPH::ShapeFilter()
  );

  REQUIRE(inside.mHits.size() == 1u);
  REQUIRE(outside.mHits.empty());
}

TEST_CASE(
  "the shape borrows the field rather than copying it",
  "[fieldshape]"
) {
  // Sixteen megabytes per body is the reason, and it is also the constraint: a
  // rebake replaces the field, so it has to rebuild the shape.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 17u);
  const FieldShape shape(field);

  REQUIRE(&shape.field() == &field);
}

namespace {

// A wide slab and a small ball resolve at very different scales in grids of
// the same size, which is the pairing contact generation used to fail on.
SampledField bakeSlab(uint32_t resolution) {
  Primitive slab{};
  slab.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)));
  slab.shape = glm::vec4(10.0f, 0.5f, 10.0f, 0.0f);
  slab.shapeConfig = glm::uvec4(1u, 1u, dunya::core::FIELD_OP_UNION, 0u);
  dunya::field::updateBounds(slab);

  return dunya::field::bake(
    std::vector<Primitive>{slab},
    glm::vec3(-10.5f, -1.0f, -10.5f),
    glm::vec3(10.5f, 1.0f, 10.5f),
    glm::uvec3(resolution)
  );
}

uint32_t contactsBetween(
  const FieldShape& one,
  const FieldShape& two,
  const glm::vec3& atOne,
  const glm::vec3& atTwo
) {
  const JPH::TransformedShape context{};

  JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
  collector.SetContext(&context);

  JPH::CollideShapeSettings settings;
  settings.mMaxSeparationDistance = 0.02f;

  JPH::CollisionDispatch::sCollideShapeVsShape(
    &one,
    &two,
    JPH::Vec3::sReplicate(1.0f),
    JPH::Vec3::sReplicate(1.0f),
    JPH::Mat44::sTranslation(JPH::Vec3(atOne.x, atOne.y, atOne.z)),
    JPH::Mat44::sTranslation(JPH::Vec3(atTwo.x, atTwo.y, atTwo.z)),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    settings,
    collector
  );

  return static_cast<uint32_t>(collector.mHits.size());
}

}  // namespace

TEST_CASE(
  "a coarse body against a fine one collides either way round",
  "[fieldshape]"
) {
  // Jolt orders a body pair by id, not by resolution, so the slot a body lands
  // in is not ours to choose. Seeding from the coarse one put nothing where the
  // fine one touches: a brick of the slab spans metres, the contact patch does
  // not, and that direction produced no contacts at all.
  JoltLibrary library;

  const SampledField slab = bakeSlab(33u);
  const SampledField ball = bakeSphere(0.5f, 65u);

  REQUIRE(slab.voxelSize.x > ball.voxelSize.x * 4.0f);

  const FieldShape slabShape(slab);
  const FieldShape ballShape(ball);

  // Pressed into the slab top face by a twentieth of the ball radius.
  const glm::vec3 slabAt(0.0f);
  const glm::vec3 ballAt(0.0f, 0.95f, 0.0f);

  const uint32_t fineFirst =
    contactsBetween(ballShape, slabShape, ballAt, slabAt);
  const uint32_t coarseFirst =
    contactsBetween(slabShape, ballShape, slabAt, ballAt);

  REQUIRE(fineFirst > 0u);
  REQUIRE(coarseFirst == fineFirst);
}

TEST_CASE("a degenerate gradient still names a direction", "[fieldshape]") {
  // With floating point exceptions unmasked on Jolt's workers, normalising a
  // zero gradient is a trap rather than a NaN, and the contact path normalises
  // whatever probe returns. A scan of 859,564 interior cells across a slab and
  // a ball found none below the floor - the weakest was 7.2e-06, at the ball's
  // own centre - so the case is reached by contract rather than by geometry,
  // and a flat field is the only honest way to ask for it.
  JoltLibrary library;

  SampledField flat = bakeSphere(1.0f, 17u);

  std::fill(flat.distances.begin(), flat.distances.end(), -1.0f);

  const dunya::field::FieldProbe probed =
    dunya::field::probe(flat, glm::vec3(0.0f, 0.0f, 0.0f));

  REQUIRE(glm::length(dunya::field::gradient(flat, glm::vec3(0.0f))) == 0.0f);
  REQUIRE_THAT(glm::length(probed.normal), WithinAbs(1.0f, 1e-6f));

  // And the shape's own accessor, which is what Jolt asks for at a contact.
  const JPH::Vec3 normal = FieldShape(flat).GetSurfaceNormal(
    JPH::SubShapeID(),
    JPH::Vec3(0.0f, 0.0f, 0.0f)
  );

  REQUIRE_THAT(normal.Length(), WithinAbs(1.0f, 1e-6f));
}
