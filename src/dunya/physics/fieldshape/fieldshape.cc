#include "fieldshape.ih"

namespace dunya::physics {

namespace {

// Nothing here is a decision about the field; it is all addressing.
using dunya::field::SampledField;

constexpr float DENSITY = 1000.0f;

// Below this a gradient names no direction. Jolt's workers run with floating
// point exceptions unmasked, so a normalize by zero traps rather than warns.
constexpr float DIRECTION_FLOOR = 1.0e-6f;

// Newton on a distance field converges in a step or two, so this is a ceiling
// rather than a budget.
constexpr uint32_t SURFACE_STEPS = 8u;
constexpr float SURFACE_TOLERANCE = 1.0e-5f;

glm::vec3 toGlm(JPH::Vec3Arg v) {
  return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

JPH::Vec3 toJph(const glm::vec3& v) {
  return JPH::Vec3(v.x, v.y, v.z);
}

glm::vec3 gridMaximum(const SampledField& field) {
  return field.origin
         + field.voxelSize * glm::vec3(field.resolution - glm::uvec3(1u));
}

// The lowest corner of a brick, in the field's own space.
glm::vec3 brickOrigin(const SampledField& field, const glm::uvec3& brick) {
  return field.origin
         + field.voxelSize * glm::vec3(brick * dunya::field::BRICK_CELLS);
}

// The coarsest axis of a grid, which is the one that bounds a contact patch it
// takes part in. Not the finest: a ground slab is thin enough to resolve
// millimetres vertically and still be a metre per brick across the plane a
// body rests on, and seeding from that puts nothing where anything touches.
float coarsestVoxel(const SampledField& field) {
  return std::max({field.voxelSize.x, field.voxelSize.y, field.voxelSize.z});
}

// Pull a point onto the field's zero set. The surface is where a contact
// lives, and a brick centre is only near it.
glm::vec3 ontoSurface(const SampledField& field, glm::vec3 point) {
  for (uint32_t step = 0; step != SURFACE_STEPS; ++step) {
    const dunya::field::FieldProbe hit = dunya::field::probe(field, point);

    if (std::fabs(hit.distance) < SURFACE_TOLERANCE) {
      break;
    }

    point -= hit.distance * hit.normal;
  }

  return point;
}

// The cached candidates that fall inside the overlap, in brick order, so the
// identifiers a manifold is keyed on do not move between frames.
template<typename Visitor>
void forEachSeed(
  const FieldShape& shape,
  const glm::vec3& overlapMinimum,
  const glm::vec3& overlapMaximum,
  Visitor&& visit
) {
  const SampledField& field = shape.field();

  const glm::uvec3 counts = dunya::field::brickCounts(field);
  const glm::vec3 span = field.voxelSize * float(dunya::field::BRICK_CELLS);

  for (const FieldSeed& seed : shape.seeds()) {
    const glm::uvec3 brick(
      seed.brick % counts.x,
      (seed.brick / counts.x) % counts.y,
      seed.brick / (counts.x * counts.y)
    );

    const glm::vec3 low = brickOrigin(field, brick);
    const glm::vec3 high = low + span;

    if (
      glm::any(glm::lessThan(high, overlapMinimum))
      || glm::any(glm::greaterThan(low, overlapMaximum))
    ) {
      continue;
    }

    visit(seed.brick, seed.point);
  }
}

// The far shape's box, brought into the near shape's space and clipped to it.
// Everything outside is a brick that cannot reach the other body this step.
//
// Grown by the separation first, because both boxes bound their solid rather
// than their grid: a speculative contact is asked for across a gap, and two
// boxes that stop at the surface do not overlap while one is still open.
JPH::AABox overlapIn(
  const FieldShape& near,
  const FieldShape& far,
  JPH::Mat44Arg centerOfMassNear,
  JPH::Mat44Arg centerOfMassFar,
  float separation
) {
  JPH::AABox brought = far.GetLocalBounds().Transformed(
    centerOfMassNear.Inversed() * centerOfMassFar
  );

  brought.ExpandBy(JPH::Vec3::sReplicate(separation));

  JPH::AABox clipped = near.GetLocalBounds();

  clipped.mMin = JPH::Vec3::sMax(clipped.mMin, brought.mMin);
  clipped.mMax = JPH::Vec3::sMin(clipped.mMax, brought.mMax);

  return clipped;
}

void collideFieldVsField(
  const JPH::Shape* inShape1,
  const JPH::Shape* inShape2,
  JPH::Vec3Arg,
  JPH::Vec3Arg,
  JPH::Mat44Arg centerOfMass1,
  JPH::Mat44Arg centerOfMass2,
  const JPH::SubShapeIDCreator& creator1,
  const JPH::SubShapeIDCreator& creator2,
  const JPH::CollideShapeSettings& settings,
  JPH::CollideShapeCollector& collector,
  const JPH::ShapeFilter&
) {
  const FieldShape& shape1 = *static_cast<const FieldShape*>(inShape1);
  const FieldShape& shape2 = *static_cast<const FieldShape*>(inShape2);

  // Seeds come from the finer body whichever slot it arrived in. A brick of a
  // coarse one is more than a metre wide, so its centres land nowhere near
  // where a small object touches it - measured as zero contacts that way round
  // against four the other. Jolt orders a pair by body, not by resolution, so
  // both ways round have to work.
  const bool flip =
    coarsestVoxel(shape2.field()) < coarsestVoxel(shape1.field());

  const FieldShape& seedShape = flip ? shape2 : shape1;
  const FieldShape& probeShape = flip ? shape1 : shape2;

  const JPH::Mat44 seedAt = flip ? centerOfMass2 : centerOfMass1;
  const JPH::Mat44 probeAt = flip ? centerOfMass1 : centerOfMass2;

  const float separation = std::max(settings.mMaxSeparationDistance, 0.0f);

  const JPH::AABox overlap =
    overlapIn(seedShape, probeShape, seedAt, probeAt, separation);

  if (!overlap.IsValid()) {
    return;
  }

  const JPH::Mat44 intoProbe = probeAt.Inversed() * seedAt;
  const JPH::Mat44 outOfProbe = probeAt.GetRotation();

  forEachSeed(
    seedShape,
    toGlm(overlap.mMin),
    toGlm(overlap.mMax),
    [&](uint32_t brick, const glm::vec3& seed) {
      if (collector.ShouldEarlyOut()) {
        return;
      }

      const JPH::Vec3 probed = intoProbe * toJph(seed);
      const dunya::field::FieldProbe hit =
        dunya::field::probe(probeShape.field(), toGlm(probed));

      if (hit.distance >= separation) {
        return;
      }

      // The probed body's outward normal, in the space both transforms share.
      const JPH::Vec3 normal = (outOfProbe * toJph(hit.normal)).Normalized();
      const JPH::Vec3 onSeed = seedAt * toJph(seed);
      const JPH::Vec3 onProbe = onSeed - hit.distance * normal;

      JPH::CollideShapeResult result;

      // mPenetrationAxis is the direction shape 2 has to move, so it follows
      // the normal when the seed is on shape 2 and opposes it when the seed is
      // on shape 1 and the normal therefore belongs to shape 2.
      result.mContactPointOn1 = flip ? onProbe : onSeed;
      result.mContactPointOn2 = flip ? onSeed : onProbe;
      result.mPenetrationAxis = flip ? normal : -normal;
      result.mPenetrationDepth = -hit.distance;

      // The brick goes in the slot the finer body occupies. A manifold is
      // keyed on both ids, so one of the two carrying the seed is enough to
      // separate contacts, and the coarse body has too few bricks to do it.
      const uint32_t bits = seedShape.GetSubShapeIDBitsRecursive();

      result.mSubShapeID1 =
        flip ? creator1.GetID() : creator1.PushID(brick, bits).GetID();
      result.mSubShapeID2 =
        flip ? creator2.PushID(brick, bits).GetID() : creator2.GetID();

      result.mBodyID2 =
        JPH::TransformedShape::sGetBodyID(collector.GetContext());

      collector.AddHit(result);
    }
  );
}

// Conservative advancement. The closest approach between the two surfaces is
// a distance the sweep provably cannot cross, so advancing by it can never
// step over a hit - and an exact field makes that distance exact.
void castFieldVsField(
  const JPH::ShapeCast& shapeCast,
  const JPH::ShapeCastSettings&,
  const JPH::Shape* inShape,
  JPH::Vec3Arg,
  const JPH::ShapeFilter&,
  JPH::Mat44Arg,
  const JPH::SubShapeIDCreator& creator1,
  const JPH::SubShapeIDCreator& creator2,
  JPH::CastShapeCollector& collector
) {
  const FieldShape& moving = *static_cast<const FieldShape*>(shapeCast.mShape);
  const FieldShape& fixed = *static_cast<const FieldShape*>(inShape);

  const float travel = shapeCast.mDirection.Length();

  if (travel < DIRECTION_FLOOR) {
    return;
  }

  const glm::vec3 everywhereLow(-std::numeric_limits<float>::max());
  const glm::vec3 everywhereHigh(std::numeric_limits<float>::max());

  float fraction = 0.0f;

  for (uint32_t iteration = 0; iteration != 64u; ++iteration) {
    const JPH::Mat44 at = shapeCast.mCenterOfMassStart.PostTranslated(
      shapeCast.mDirection * fraction
    );

    float closest = std::numeric_limits<float>::max();
    JPH::Vec3 closestPoint = JPH::Vec3::sZero();
    glm::vec3 closestNormal(0.0f, 1.0f, 0.0f);
    uint32_t closestBrick = 0u;

    forEachSeed(
      moving,
      everywhereLow,
      everywhereHigh,
      [&](uint32_t brick, const glm::vec3& seed) {
        const JPH::Vec3 probed = at * toJph(seed);
        const dunya::field::FieldProbe hit =
          dunya::field::probe(fixed.field(), toGlm(probed));

        if (hit.distance < closest) {
          closest = hit.distance;
          closestPoint = probed;
          closestNormal = hit.normal;
          closestBrick = brick;
        }
      }
    );

    if (closest <= 1.0e-3f) {
      JPH::ShapeCastResult result;

      result.mFraction = fraction;
      result.mContactPointOn1 = closestPoint;
      result.mContactPointOn2 = closestPoint - closest * toJph(closestNormal);
      result.mPenetrationAxis = -toJph(closestNormal);
      result.mPenetrationDepth = -closest;
      result.mSubShapeID1 =
        creator1.PushID(closestBrick, moving.GetSubShapeIDBitsRecursive())
          .GetID();
      result.mSubShapeID2 = creator2.GetID();
      result.mIsBackFaceHit = false;
      result.mBodyID2 =
        JPH::TransformedShape::sGetBodyID(collector.GetContext());

      collector.AddHit(result);

      return;
    }

    fraction += closest / travel;

    if (fraction >= 1.0f) {
      return;
    }
  }
}

}  // namespace

FieldShape::FieldShape(const dunya::field::SampledField& field)
    : JPH::Shape(JPH::EShapeType::User1, JPH::EShapeSubType::User1),
      m_field(&field) {
  const glm::uvec3 counts = dunya::field::brickCounts(field);
  const uint64_t bricks = static_cast<uint64_t>(counts.x) * counts.y * counts.z;

  while ((uint64_t{1} << m_subShapeBits) < bricks) {
    ++m_subShapeBits;
  }

  if (m_subShapeBits > JPH::SubShapeID::MaxBits) {
    throw std::runtime_error(
      "FieldShape: the grid needs more sub shape bits than Jolt has"
    );
  }

  // The inscribed sphere: the deepest the field goes is how far the shape can
  // move without risking passing through something. Jolt asserts on a zero.
  float deepest = 0.0f;

  for (float low : field.brickMinimum) {
    deepest = std::min(deepest, low);
  }

  m_innerRadius = std::max(-deepest, 1.0e-3f);

  // One pass for both derived things. The contact candidates are solved here,
  // once, rather than inside the collide and sweep paths: the answer depends
  // on the field alone, and a shape is immutable. Eagerly, so nothing is
  // written while Jolt's workers are reading it.
  const glm::vec3 span = field.voxelSize * float(dunya::field::BRICK_CELLS);

  glm::vec3 solidLow(std::numeric_limits<float>::max());
  glm::vec3 solidHigh(std::numeric_limits<float>::lowest());

  bool anySolid = false;

  for (uint32_t index = 0u; index != bricks; ++index) {
    // A brick whose range never goes negative holds nothing. The ranges are
    // taken over a one-cell halo, so a brick positive throughout has a
    // positive interpolant throughout and the solid cannot reach into it.
    if (field.brickMinimum[index] > 0.0f) {
      continue;
    }

    const glm::uvec3 brick(
      index % counts.x,
      (index / counts.x) % counts.y,
      index / (counts.x * counts.y)
    );

    const glm::vec3 corner = brickOrigin(field, brick);

    solidLow = glm::min(solidLow, corner);
    solidHigh = glm::max(solidHigh, corner + span);

    anySolid = true;

    if (!dunya::field::brickHoldsSurface(field, index)) {
      continue;
    }

    m_seeds.push_back({ontoSurface(field, corner + span * 0.5f), index});
  }

  // The solid's extent, not the grid's. A grid carries whatever margin it was
  // baked with - half a metre around a ball the size of a fist - and every
  // broad phase pair that margin wins is a seed walk that cannot touch.
  //
  // Clamped to the grid, because the last brick on an axis runs past the far
  // lattice point whenever the cell count is not a multiple of the brick size.
  if (anySolid) {
    m_bounds = JPH::AABox(
      toJph(glm::max(solidLow, field.origin)),
      toJph(glm::min(solidHigh, gridMaximum(field)))
    );

    return;
  }

  // Nothing solid anywhere, so there is no extent to be tighter than and the
  // grid is all this shape can answer for.
  m_bounds = JPH::AABox(toJph(field.origin), toJph(gridMaximum(field)));
}

const dunya::field::SampledField& FieldShape::field() const noexcept {
  return *m_field;
}

std::span<const FieldSeed> FieldShape::seeds() const noexcept {
  return m_seeds;
}

JPH::AABox FieldShape::GetLocalBounds() const {
  return m_bounds;
}

JPH::uint FieldShape::GetSubShapeIDBitsRecursive() const {
  return m_subShapeBits;
}

float FieldShape::GetInnerRadius() const {
  return m_innerRadius;
}

JPH::MassProperties FieldShape::GetMassProperties() const {
  if (m_massKnown) {
    return m_massProperties;
  }

  // Summed over the voxels, which is the one thing a volume representation
  // makes trivial: no hollow-versus-solid guess, no hand tuning.
  const glm::uvec3 cells = m_field->resolution - glm::uvec3(1u);
  const glm::uvec3 bricks = dunya::field::brickCounts(*m_field);

  const float cellVolume =
    m_field->voxelSize.x * m_field->voxelSize.y * m_field->voxelSize.z;
  const float cellMass = cellVolume * DENSITY;

  float mass = 0.0f;
  glm::dmat3 secondMoment(0.0);

  const uint32_t brickCount = bricks.x * bricks.y * bricks.z;

  for (uint32_t brick = 0u; brick != brickCount; ++brick) {
    // The value range answers both questions before a single sample: a brick
    // that never goes negative holds nothing, and one that never goes positive
    // holds nothing else. Only the boundary is worth walking.
    if (m_field->brickMinimum[brick] > 0.0f) {
      continue;
    }

    const bool solid = m_field->brickMaximum[brick] < 0.0f;

    const glm::uvec3 at(
      brick % bricks.x,
      (brick / bricks.x) % bricks.y,
      brick / (bricks.x * bricks.y)
    );

    const glm::uvec3 base = at * glm::uvec3(dunya::field::BRICK_CELLS);
    const glm::uvec3 last =
      glm::min(base + glm::uvec3(dunya::field::BRICK_CELLS), cells);

    for (uint32_t z = base.z; z < last.z; ++z) {
      for (uint32_t y = base.y; y < last.y; ++y) {
        for (uint32_t x = base.x; x < last.x; ++x) {
          const glm::vec3 centre =
            m_field->origin
            + m_field->voxelSize * (glm::vec3(x, y, z) + glm::vec3(0.5f));

          if (!solid && dunya::field::distance(*m_field, centre) >= 0.0f) {
            continue;
          }

          mass += cellMass;

          const glm::dvec3 p(centre);

          secondMoment[0][0] += cellMass * (p.y * p.y + p.z * p.z);
          secondMoment[1][1] += cellMass * (p.x * p.x + p.z * p.z);
          secondMoment[2][2] += cellMass * (p.x * p.x + p.y * p.y);
          secondMoment[0][1] -= cellMass * p.x * p.y;
          secondMoment[0][2] -= cellMass * p.x * p.z;
          secondMoment[1][2] -= cellMass * p.y * p.z;
        }
      }
    }
  }

  JPH::MassProperties properties;

  if (mass <= 0.0f) {
    // Nothing solid in the grid. A body on this must supply its own mass.
    m_massProperties = properties;
    m_massKnown = true;

    return m_massProperties;
  }

  secondMoment[1][0] = secondMoment[0][1];
  secondMoment[2][0] = secondMoment[0][2];
  secondMoment[2][1] = secondMoment[1][2];

  properties.mMass = mass;
  properties.mInertia = JPH::Mat44::sIdentity();

  for (int column = 0; column != 3; ++column) {
    for (int row = 0; row != 3; ++row) {
      properties.mInertia(row, column) =
        static_cast<float>(secondMoment[column][row]);
    }
  }

  m_massProperties = properties;
  m_massKnown = true;

  return m_massProperties;
}

const JPH::PhysicsMaterial* FieldShape::GetMaterial(
  const JPH::SubShapeID&
) const {
  return JPH::PhysicsMaterial::sDefault;
}

JPH::Vec3 FieldShape::GetSurfaceNormal(
  const JPH::SubShapeID&,
  JPH::Vec3Arg localSurfacePosition
) const {
  return toJph(
    dunya::field::probe(*m_field, toGlm(localSurfacePosition)).normal
  );
}

void FieldShape::GetSubmergedVolume(
  JPH::Mat44Arg,
  JPH::Vec3Arg,
  const JPH::Plane&,
  float& totalVolume,
  float& submergedVolume,
  JPH::Vec3& centerOfBuoyancy
#ifdef JPH_DEBUG_RENDERER
  ,
  JPH::RVec3Arg
#endif
) const {
  // Buoyancy is not modelled. Reporting nothing submerged is the honest
  // answer, and the only caller is ApplyBuoyancyImpulse.
  totalVolume = 0.0f;
  submergedVolume = 0.0f;
  centerOfBuoyancy = JPH::Vec3::sZero();
}

#ifdef JPH_DEBUG_RENDERER

void FieldShape::Draw(
  JPH::DebugRenderer*,
  JPH::RMat44Arg,
  JPH::Vec3Arg,
  JPH::ColorArg,
  bool,
  bool
) const {
  // The renderer already draws the field; Jolt's debug view would only
  // duplicate it, and there is no triangle list to hand it anyway.
}

#endif

bool FieldShape::CastRay(
  const JPH::RayCast&,
  const JPH::SubShapeIDCreator&,
  JPH::RayCastResult&
) const {
  // Queries against a field object are not wired yet. Reporting no hit is
  // wrong only for a caller that does not exist.
  return false;
}

void FieldShape::CastRay(
  const JPH::RayCast&,
  const JPH::RayCastSettings&,
  const JPH::SubShapeIDCreator&,
  JPH::CastRayCollector&,
  const JPH::ShapeFilter&
) const {}

void FieldShape::CollidePoint(
  JPH::Vec3Arg point,
  const JPH::SubShapeIDCreator& subShapeIDCreator,
  JPH::CollidePointCollector& collector,
  const JPH::ShapeFilter&
) const {
  // One field evaluation, which is the whole test.
  if (dunya::field::probe(*m_field, toGlm(point)).distance >= 0.0f) {
    return;
  }

  collector.AddHit(
    {JPH::TransformedShape::sGetBodyID(collector.GetContext()),
     subShapeIDCreator.GetID()}
  );
}

void FieldShape::CollideSoftBodyVertices(
  JPH::Mat44Arg,
  JPH::Vec3Arg,
  const JPH::CollideSoftBodyVertexIterator&,
  JPH::uint,
  int
) const {}

void FieldShape::GetTrianglesStart(
  GetTrianglesContext&,
  const JPH::AABox&,
  JPH::Vec3Arg,
  JPH::QuatArg,
  JPH::Vec3Arg
) const {}

int FieldShape::GetTrianglesNext(
  GetTrianglesContext&,
  int,
  JPH::Float3*,
  const JPH::PhysicsMaterial**
) const {
  // There is no triangle list. Jolt only walks this for debug drawing.
  return 0;
}

JPH::Shape::Stats FieldShape::GetStats() const {
  return Stats(sizeof(*this) + m_field->distances.size() * sizeof(float), 0u);
}

float FieldShape::GetVolume() const {
  // Aggregated by compound shapes and nothing else; the shipped non-convex
  // shapes all return zero for the same reason.
  return 0.0f;
}

void registerFieldShape() {
  JPH::ShapeFunctions& functions =
    JPH::ShapeFunctions::sGet(JPH::EShapeSubType::User1);

  // Never null: Shape::sRestoreFromBinaryState calls it unguarded.
  functions.mConstruct = []() -> JPH::Shape* {
    return nullptr;
  };
  functions.mColor = JPH::Color::sGreen;

  JPH::CollisionDispatch::sRegisterCollideShape(
    JPH::EShapeSubType::User1,
    JPH::EShapeSubType::User1,
    collideFieldVsField
  );

  JPH::CollisionDispatch::sRegisterCastShape(
    JPH::EShapeSubType::User1,
    JPH::EShapeSubType::User1,
    castFieldVsField
  );
}

}  // namespace dunya::physics
