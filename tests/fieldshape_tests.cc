#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fieldprimitives.h"

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

constexpr float DENSITY = 1000.0f;

// One sphere in a box that clears it by half a radius, so the grid holds
// outside as well as inside.
SampledField bakeSphere(
  float radius,
  uint32_t resolution,
  const glm::vec3& centre = glm::vec3(0.0f)
) {
  const float reach = radius * 1.5f;

  return dunya::field::bake(
    std::vector<Primitive>{fixture::sphere(centre, radius)},
    centre - glm::vec3(reach),
    centre + glm::vec3(reach),
    glm::uvec3(resolution)
  );
}

float sphereMass(float radius) {
  return DENSITY * 4.0f / 3.0f * glm::pi<float>() * radius * radius * radius;
}

}  // namespace

TEST_CASE("the local bounds are the solid, not the grid", "[fieldshape]") {
  // Jolt culls on this box. A grid carries whatever margin it was baked with,
  // and every broad phase pair that margin wins is a seed walk that cannot
  // reach - but one voxel too small drops contacts, so it has to contain the
  // solid. This grid reaches 1.5 around a sphere of radius 1.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u);
  const FieldShape shape(field);

  const JPH::AABox bounds = shape.GetLocalBounds();

  for (int axis = 0; axis != 3; ++axis) {
    REQUIRE(bounds.mMin[axis] <= -1.0f);
    REQUIRE(bounds.mMax[axis] >= 1.0f);

    REQUIRE(bounds.mMin[axis] > -1.5f);
    REQUIRE(bounds.mMax[axis] < 1.5f);
  }
}

TEST_CASE(
  "a solid away from the origin says where its mass is",
  "[fieldshape]"
) {
  // Jolt turns a body about its centre of mass and expresses every transform
  // it hands a shape in that space. A shape that answers zero for a solid
  // three metres off its own origin is simulated three metres from where its
  // geometry is: it culls in the wrong place and every contact is torque.
  JoltLibrary library;

  const glm::vec3 offset(0.0f, 3.0f, 0.0f);

  const SampledField field = bakeSphere(1.0f, 65u, offset);
  const FieldShape shape(field);

  const JPH::Vec3 centre = shape.GetCenterOfMass();

  REQUIRE_THAT(centre.GetX(), WithinAbs(offset.x, 0.01f));
  REQUIRE_THAT(centre.GetY(), WithinAbs(offset.y, 0.01f));
  REQUIRE_THAT(centre.GetZ(), WithinAbs(offset.z, 0.01f));
}

TEST_CASE("an offset solid is bounded about its own centre", "[fieldshape]") {
  // The bounds are read in centre of mass space, so a sphere of radius 1 three
  // metres up still straddles zero. Left in field space it would be a box from
  // 2 to 4, and Jolt would cull the body where nothing is.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u, glm::vec3(0.0f, 3.0f, 0.0f));
  const FieldShape shape(field);

  const JPH::AABox bounds = shape.GetLocalBounds();

  for (int axis = 0; axis != 3; ++axis) {
    REQUIRE(bounds.mMin[axis] <= -1.0f);
    REQUIRE(bounds.mMax[axis] >= 1.0f);
    REQUIRE(bounds.mMin[axis] > -1.5f);
    REQUIRE(bounds.mMax[axis] < 1.5f);
  }
}

TEST_CASE(
  "an offset solid resists turning like its own shape",
  "[fieldshape]"
) {
  // The walk sums about the field's origin, so an offset solid picks up a
  // parallel axis term of m*d*d - here nine times the sphere's own, which
  // would make a ball behave like a weight on the end of a three metre bar.
  JoltLibrary library;

  const SampledField centred = bakeSphere(1.0f, 65u);
  const SampledField offset =
    bakeSphere(1.0f, 65u, glm::vec3(0.0f, 3.0f, 0.0f));

  const JPH::MassProperties here = FieldShape(centred).GetMassProperties();
  const JPH::MassProperties there = FieldShape(offset).GetMassProperties();

  REQUIRE_THAT(there.mMass, WithinRel(here.mMass, 0.02f));

  for (int axis = 0; axis != 3; ++axis) {
    REQUIRE_THAT(
      there.mInertia(axis, axis),
      WithinRel(here.mInertia(axis, axis), 0.02f)
    );
  }
}

TEST_CASE("an offset solid is asked about in the right space", "[fieldshape]") {
  // CollidePoint takes a point in centre of mass space. The sphere's own
  // centre is the origin there, and the point three metres up - which is where
  // the solid sits in field space - is outside it.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u, glm::vec3(0.0f, 3.0f, 0.0f));
  const FieldShape shape(field);

  JPH::AllHitCollisionCollector<JPH::CollidePointCollector> inside;
  shape.CollidePoint(JPH::Vec3::sZero(), JPH::SubShapeIDCreator(), inside, {});

  REQUIRE(inside.mHits.size() == 1u);

  JPH::AllHitCollisionCollector<JPH::CollidePointCollector> outside;
  shape.CollidePoint(
    JPH::Vec3(0.0f, 3.0f, 0.0f),
    JPH::SubShapeIDCreator(),
    outside,
    {}
  );

  REQUIRE(outside.mHits.empty());
}

TEST_CASE("an empty grid answers for its whole box", "[fieldshape]") {
  // There is no solid to be tighter than, and a degenerate box is a body Jolt
  // can never find. The grid is what the field can answer for, so it is that.
  JoltLibrary library;

  const SampledField field = dunya::field::bake(
    std::vector<Primitive>{fixture::sphere(glm::vec3(10.0f), 0.5f)},
    glm::vec3(-1.0f),
    glm::vec3(1.0f),
    glm::uvec3(17u)
  );

  const FieldShape shape(field);
  const JPH::AABox bounds = shape.GetLocalBounds();

  for (int axis = 0; axis != 3; ++axis) {
    REQUIRE_THAT(bounds.mMin[axis], WithinAbs(field.origin[axis], 1e-6f));
    REQUIRE_THAT(
      bounds.mMax[axis],
      WithinAbs(
        field.origin[axis]
          + field.voxelSize[axis] * float(field.resolution[axis] - 1u),
        1e-6f
      )
    );
  }
}

TEST_CASE(
  "the seeds are on the surface, one per surface brick",
  "[fieldshape]"
) {
  // Solved with the shape and read from there by every collide and every
  // iteration of every sweep, so what they are has to hold before any query is
  // made. One per brick that holds surface, in brick order, on the zero set.
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u);
  const FieldShape shape(field);

  const glm::uvec3 counts = dunya::field::brickCounts(field);
  const uint32_t bricks = counts.x * counts.y * counts.z;

  uint32_t holding = 0u;

  for (uint32_t brick = 0u; brick != bricks; ++brick) {
    if (dunya::field::brickHoldsSurface(field, brick)) {
      ++holding;
    }
  }

  REQUIRE(holding > 0u);
  REQUIRE(shape.seeds().size() == holding);

  float previous = -1.0f;

  for (const dunya::physics::FieldSeed& seed : shape.seeds()) {
    REQUIRE(dunya::field::brickHoldsSurface(field, seed.brick));
    REQUIRE(float(seed.brick) > previous);

    previous = float(seed.brick);

    REQUIRE_THAT(
      dunya::field::distance(field, seed.point),
      WithinAbs(0.0f, 1.0e-4f)
    );
  }
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
    std::vector<Primitive>{fixture::sphere(glm::vec3(10.0f), 0.5f)},
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

TEST_CASE(
  "each shape keeps its own mass, not the last one asked",
  "[fieldshape]"
) {
  // The walk is kept after the first ask, since every body built on a shape
  // asks again. Kept on the shape: a cache that outlived one would hand every
  // later shape the first shape's answer, and a ball would weigh a ground.
  JoltLibrary library;

  const SampledField small = bakeSphere(0.5f, 65u);
  const SampledField large = bakeSphere(1.0f, 65u);

  const FieldShape smallShape(small);
  const FieldShape largeShape(large);

  const float smallMass = smallShape.GetMassProperties().mMass;
  const float largeMass = largeShape.GetMassProperties().mMass;

  REQUIRE_THAT(smallMass, WithinRel(sphereMass(0.5f), 0.05f));
  REQUIRE_THAT(largeMass, WithinRel(sphereMass(1.0f), 0.05f));

  // Asked again, in the other order, which is where a shared cache shows.
  REQUIRE(largeShape.GetMassProperties().mMass == largeMass);
  REQUIRE(smallShape.GetMassProperties().mMass == smallMass);
}

TEST_CASE("a solid box weighs and turns like a solid box", "[fieldshape]") {
  // A box is nearly all interior, which is the case the walk answers with a
  // closed form rather than by visiting five hundred cells a brick. Checked
  // against the analytic box rather than against the walk it replaced: mass
  // 8*hx*hy*hz*rho, and m*(hb^2 + hc^2)/3 about each axis.
  JoltLibrary library;

  const glm::vec3 half(0.6f, 0.4f, 0.3f);

  Primitive box{};
  box.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)));
  box.shape = glm::vec4(half, 0.0f);
  box.shapeConfig = glm::uvec4(1u, 1u, dunya::core::FIELD_OP_UNION, 0u);
  dunya::field::updateBounds(box);

  const SampledField field = dunya::field::bake(
    std::vector<Primitive>{box},
    -half - glm::vec3(0.25f),
    half + glm::vec3(0.25f),
    glm::uvec3(65u)
  );

  const JPH::MassProperties properties = FieldShape(field).GetMassProperties();

  const float expected = DENSITY * 8.0f * half.x * half.y * half.z;

  REQUIRE_THAT(properties.mMass, WithinRel(expected, 0.03f));

  const glm::vec3 squared = half * half;

  REQUIRE_THAT(
    properties.mInertia(0, 0),
    WithinRel(properties.mMass * (squared.y + squared.z) / 3.0f, 0.03f)
  );
  REQUIRE_THAT(
    properties.mInertia(1, 1),
    WithinRel(properties.mMass * (squared.x + squared.z) / 3.0f, 0.03f)
  );
  REQUIRE_THAT(
    properties.mInertia(2, 2),
    WithinRel(properties.mMass * (squared.x + squared.y) / 3.0f, 0.03f)
  );

  // Off centre, because the closed form places cells by a different route than
  // the per-cell walk and a wrong one shows up as a shifted centre of mass.
  REQUIRE_THAT(
    FieldShape(field).GetCenterOfMass().GetX(),
    WithinAbs(0.0f, 0.005f)
  );
  REQUIRE_THAT(
    FieldShape(field).GetCenterOfMass().GetY(),
    WithinAbs(0.0f, 0.005f)
  );
  REQUIRE_THAT(
    FieldShape(field).GetCenterOfMass().GetZ(),
    WithinAbs(0.0f, 0.005f)
  );
}

TEST_CASE("carving the solid takes mass with it", "[fieldshape]") {
  // The property denting depends on: mass follows the geometry, so a body
  // that loses material loses weight without anyone maintaining a number.
  JoltLibrary library;

  const SampledField whole = bakeSphere(1.0f, 65u);

  const SampledField carved = dunya::field::bake(
    std::vector<Primitive>{
      fixture::sphere(glm::vec3(0.0f), 1.0f),
      fixture::sphere(
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
