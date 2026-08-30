#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fieldprimitives.h"

#include <dunya/core/config/config.h>
#include <dunya/field/analytic/analytic.h>
#include <dunya/field/deform/deform.h>
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

}

TEST_CASE("the local bounds are the solid, not the grid", "[fieldshape]") {
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
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 33u);
  const FieldShape shape(field);

  REQUIRE_THAT(shape.GetInnerRadius(), WithinAbs(1.0f, field.voxelSize.x));
}

TEST_CASE("an empty grid still reports a usable inner radius", "[fieldshape]") {
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
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 65u);
  const FieldShape shape(field);

  REQUIRE_THAT(
    shape.GetMassProperties().mMass,
    WithinRel(sphereMass(1.0f), 0.05f)
  );
}

TEST_CASE("the inertia is the solid's, about its own axes", "[fieldshape]") {
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
  JoltLibrary library;

  const SampledField small = bakeSphere(0.5f, 65u);
  const SampledField large = bakeSphere(1.0f, 65u);

  const FieldShape smallShape(small);
  const FieldShape largeShape(large);

  const float smallMass = smallShape.GetMassProperties().mMass;
  const float largeMass = largeShape.GetMassProperties().mMass;

  REQUIRE_THAT(smallMass, WithinRel(sphereMass(0.5f), 0.05f));
  REQUIRE_THAT(largeMass, WithinRel(sphereMass(1.0f), 0.05f));

  REQUIRE(largeShape.GetMassProperties().mMass == largeMass);
  REQUIRE(smallShape.GetMassProperties().mMass == smallMass);
}

TEST_CASE("a solid box weighs and turns like a solid box", "[fieldshape]") {
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

  REQUIRE(lost > sphereMass(0.5f) * 0.5f);
  REQUIRE(lost < sphereMass(0.5f));
}

TEST_CASE("every brick has a sub shape id of its own", "[fieldshape]") {
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
  JoltLibrary library;

  const SampledField field = bakeSphere(1.0f, 17u);
  const FieldShape shape(field);

  REQUIRE(&shape.field() == &field);
}

namespace {

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

}

TEST_CASE(
  "a coarse body against a fine one collides either way round",
  "[fieldshape]"
) {
  JoltLibrary library;

  const SampledField slab = bakeSlab(33u);
  const SampledField ball = bakeSphere(0.5f, 65u);

  REQUIRE(slab.voxelSize.x > ball.voxelSize.x * 4.0f);

  const FieldShape slabShape(slab);
  const FieldShape ballShape(ball);

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
  JoltLibrary library;

  SampledField flat = bakeSphere(1.0f, 17u);

  std::fill(flat.distances.begin(), flat.distances.end(), -1.0f);

  const dunya::field::FieldProbe probed =
    dunya::field::probe(flat, glm::vec3(0.0f, 0.0f, 0.0f));

  REQUIRE(glm::length(dunya::field::gradient(flat, glm::vec3(0.0f))) == 0.0f);
  REQUIRE_THAT(glm::length(probed.normal), WithinAbs(1.0f, 1e-6f));

  const JPH::Vec3 normal = FieldShape(flat).GetSurfaceNormal(
    JPH::SubShapeID(),
    JPH::Vec3(0.0f, 0.0f, 0.0f)
  );

  REQUIRE_THAT(normal.Length(), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE(
  "a shape rebuilt from a deformation matches one built from scratch",
  "[fieldshape]"
) {
  JoltLibrary library;

  const std::vector<Primitive> primitives{
    fixture::sphere(glm::vec3(0.0f), 1.0f)
  };

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-1.6f),
    glm::vec3(1.6f),
    glm::uvec3(65u)
  );

  const JPH::Ref<FieldShape> before = new FieldShape(field);

  Primitive cutter = dunya::field::makeSphere(
    glm::vec3(0.9f, 0.0f, 0.0f),
    0.4f,
    0u,
    dunya::core::FIELD_OP_SUBTRACTION
  );

  dunya::field::updateBounds(cutter);

  const dunya::field::WriteReport report =
    dunya::field::deformAndRepair(field, cutter).write;

  REQUIRE(report.brickEnd.x > report.brickBegin.x);

  const JPH::Ref<FieldShape> patched =
    new FieldShape(field, *before, report.brickBegin, report.brickEnd);

  const JPH::Ref<FieldShape> scratch = new FieldShape(field);

  const JPH::MassProperties wanted = scratch->GetMassProperties();
  const JPH::MassProperties got = patched->GetMassProperties();

  REQUIRE(wanted.mMass < before->GetMassProperties().mMass * 0.999f);

  REQUIRE_THAT(got.mMass, WithinRel(wanted.mMass, 1.0e-6f));

  REQUIRE_THAT(
    patched->centerOfMass().x,
    WithinAbs(scratch->centerOfMass().x, 1.0e-6f)
  );
  REQUIRE_THAT(
    patched->centerOfMass().y,
    WithinAbs(scratch->centerOfMass().y, 1.0e-6f)
  );
  REQUIRE_THAT(
    patched->centerOfMass().z,
    WithinAbs(scratch->centerOfMass().z, 1.0e-6f)
  );

  for (int column = 0; column != 3; ++column) {
    for (int row = 0; row != 3; ++row) {
      REQUIRE_THAT(
        got.mInertia(row, column),
        WithinAbs(wanted.mInertia(row, column), 1.0e-3f)
      );
    }
  }

  REQUIRE(patched->seeds().size() == scratch->seeds().size());

  for (size_t i = 0; i < patched->seeds().size(); ++i) {
    REQUIRE(patched->seeds()[i].brick == scratch->seeds()[i].brick);

    REQUIRE_THAT(
      patched->seeds()[i].point.x,
      WithinAbs(scratch->seeds()[i].point.x, 1.0e-5f)
    );
    REQUIRE_THAT(
      patched->seeds()[i].point.y,
      WithinAbs(scratch->seeds()[i].point.y, 1.0e-5f)
    );
    REQUIRE_THAT(
      patched->seeds()[i].point.z,
      WithinAbs(scratch->seeds()[i].point.z, 1.0e-5f)
    );
  }

  const JPH::Ref<FieldShape> allReused =
    new FieldShape(field, *before, glm::uvec3(0u), glm::uvec3(0u));

  REQUIRE(
    allReused->GetMassProperties().mMass == before->GetMassProperties().mMass
  );
  REQUIRE(allReused->GetMassProperties().mMass > wanted.mMass);
}
