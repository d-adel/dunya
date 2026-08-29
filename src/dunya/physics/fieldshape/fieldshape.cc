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

// What the solid weighs, where it is, and how it resists turning. The second
// moment is about the field's own origin; a body turns about its centre of
// mass, so the shift to that point happens once the centre is known.
struct SolidIntegral {
  double mass = 0.0;
  glm::dvec3 firstMoment{0.0};
  glm::dmat3 secondMoment{0.0};
};

// Summed over the voxels, which is the one thing a volume representation makes
// trivial: no hollow-versus-solid guess, no hand tuning.
// What a run of cell centres on one axis contributes: how many there are,
// where they are, and where they are squared. A brick that is solid throughout
// needs no per-cell test, and these three numbers stand in for walking it.
struct AxisSums {
  double count = 0.0;
  double first = 0.0;
  double second = 0.0;
};

AxisSums axisSums(float origin, float voxel, uint32_t begin, uint32_t end) {
  AxisSums sums;

  for (uint32_t i = begin; i != end; ++i) {
    // In float first, so a solid brick and a boundary one place their cells at
    // the same coordinates rather than at two roundings of them.
    const double at = double(origin + voxel * (float(i) + 0.5f));

    sums.count += 1.0;
    sums.first += at;
    sums.second += at * at;
  }

  return sums;
}

// Summed over the voxels, which is the one thing a volume representation makes
// trivial: no hollow-versus-solid guess, no hand tuning.
SolidIntegral integrateSolid(const SampledField& field) {
  const glm::uvec3 cells = field.resolution - glm::uvec3(1u);
  const glm::uvec3 bricks = dunya::field::brickCounts(field);

  const float cellVolume =
    field.voxelSize.x * field.voxelSize.y * field.voxelSize.z;
  const double cellMass = double(cellVolume) * DENSITY;

  // A cell's eight corners, as offsets from the lattice index of its lowest.
  const uint32_t alongY = field.resolution.x;
  const uint32_t alongZ = field.resolution.x * field.resolution.y;

  SolidIntegral solid;

  const uint32_t brickCount = bricks.x * bricks.y * bricks.z;

  for (uint32_t brick = 0u; brick != brickCount; ++brick) {
    // The value range answers both questions before a single sample: a brick
    // that never goes negative holds nothing, and one that never goes positive
    // holds nothing else. Only the boundary is worth walking.
    if (field.brickMinimum[brick] > 0.0f) {
      continue;
    }

    const glm::uvec3 at(
      brick % bricks.x,
      (brick / bricks.x) % bricks.y,
      brick / (bricks.x * bricks.y)
    );

    const glm::uvec3 base = at * glm::uvec3(dunya::field::BRICK_CELLS);
    const glm::uvec3 last =
      glm::min(base + glm::uvec3(dunya::field::BRICK_CELLS), cells);

    if (field.brickMaximum[brick] < 0.0f) {
      // Every cell in it is inside, so what the walk would sum has a closed
      // form. The ranges are taken over a one-cell halo, which is what makes
      // that safe out to the brick's last cell.
      const AxisSums sx =
        axisSums(field.origin.x, field.voxelSize.x, base.x, last.x);
      const AxisSums sy =
        axisSums(field.origin.y, field.voxelSize.y, base.y, last.y);
      const AxisSums sz =
        axisSums(field.origin.z, field.voxelSize.z, base.z, last.z);

      const double xx = sx.second * sy.count * sz.count;
      const double yy = sx.count * sy.second * sz.count;
      const double zz = sx.count * sy.count * sz.second;

      solid.mass += cellMass * sx.count * sy.count * sz.count;

      solid.firstMoment += cellMass
                           * glm::dvec3(
                             sx.first * sy.count * sz.count,
                             sx.count * sy.first * sz.count,
                             sx.count * sy.count * sz.first
                           );

      solid.secondMoment[0][0] += cellMass * (yy + zz);
      solid.secondMoment[1][1] += cellMass * (xx + zz);
      solid.secondMoment[2][2] += cellMass * (xx + yy);
      solid.secondMoment[0][1] -= cellMass * sx.first * sy.first * sz.count;
      solid.secondMoment[0][2] -= cellMass * sx.first * sy.count * sz.first;
      solid.secondMoment[1][2] -= cellMass * sx.count * sy.first * sz.first;

      continue;
    }

    for (uint32_t z = base.z; z < last.z; ++z) {
      for (uint32_t y = base.y; y < last.y; ++y) {
        for (uint32_t x = base.x; x < last.x; ++x) {
          const uint32_t lowest = x + alongY * y + alongZ * z;

          // Trilinear interpolation at the middle of a cell is the mean of its
          // eight corners, so the sample the walk needs is a sum of eight
          // values and no coordinate arithmetic at all. Only its sign is read,
          // which is why it is never divided by eight.
          const float corners =
            field.distances[lowest] + field.distances[lowest + 1u]
            + field.distances[lowest + alongY]
            + field.distances[lowest + alongY + 1u]
            + field.distances[lowest + alongZ]
            + field.distances[lowest + alongZ + 1u]
            + field.distances[lowest + alongZ + alongY]
            + field.distances[lowest + alongZ + alongY + 1u];

          if (corners >= 0.0f) {
            continue;
          }

          const glm::vec3 centre =
            field.origin
            + field.voxelSize * (glm::vec3(x, y, z) + glm::vec3(0.5f));

          solid.mass += cellMass;

          const glm::dvec3 p(centre);

          solid.firstMoment += cellMass * p;

          solid.secondMoment[0][0] += cellMass * (p.y * p.y + p.z * p.z);
          solid.secondMoment[1][1] += cellMass * (p.x * p.x + p.z * p.z);
          solid.secondMoment[2][2] += cellMass * (p.x * p.x + p.y * p.y);
          solid.secondMoment[0][1] -= cellMass * p.x * p.y;
          solid.secondMoment[0][2] -= cellMass * p.x * p.z;
          solid.secondMoment[1][2] -= cellMass * p.y * p.z;
        }
      }
    }
  }

  return solid;
}

// The cached candidates that fall inside the overlap, in brick order, so the
// identifiers a manifold is keyed on do not move between frames.
//
// Tested by the candidate itself rather than by the brick it is named after:
// pulling a brick centre onto the surface is under no obligation to stay
// inside that brick, and measured over the suite one seed in seven does not -
// by up to a sixth of a brick. Testing the point is also exactly the
// condition that matters, since a candidate outside the far body's box grown
// by the separation is further away than a contact is allowed to be.
template<typename Visitor>
void forEachSeed(
  const FieldShape& shape,
  const glm::vec3& overlapMinimum,
  const glm::vec3& overlapMaximum,
  Visitor&& visit
) {
  for (const FieldSeed& seed : shape.seeds()) {
    if (
      glm::any(glm::lessThan(seed.point, overlapMinimum))
      || glm::any(glm::greaterThan(seed.point, overlapMaximum))
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

  // Back into the near shape's field space, which is where its bricks are.
  clipped.Translate(toJph(near.centerOfMass()));

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

      const glm::vec3 fromSeedCentre = seed - seedShape.centerOfMass();

      const JPH::Vec3 probed = intoProbe * toJph(fromSeedCentre);
      const dunya::field::FieldProbe hit = dunya::field::probe(
        probeShape.field(),
        toGlm(probed) + probeShape.centerOfMass()
      );

      if (hit.distance >= separation) {
        return;
      }

      // The probed body's outward normal, in the space both transforms share.
      const JPH::Vec3 normal = (outOfProbe * toJph(hit.normal)).Normalized();
      const JPH::Vec3 onSeed = seedAt * toJph(fromSeedCentre);
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
        const JPH::Vec3 probed = at * toJph(seed - moving.centerOfMass());
        const dunya::field::FieldProbe hit = dunya::field::probe(
          fixed.field(),
          toGlm(probed) + fixed.centerOfMass()
        );

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
  // Everything below indexes the lattice and the brick ranges. An unbaked
  // field has neither, and resolution zero would run the cell count backwards
  // past four billion before anyone noticed.
  if (glm::any(glm::lessThan(field.resolution, glm::uvec3(2u)))) {
    throw std::runtime_error(
      "FieldShape: the field has no cells to collide with"
    );
  }

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

  // Where the solid is, what it weighs, and how it resists turning - all one
  // walk, and all needed before any query: Jolt asks for the centre of mass
  // while creating the body and expresses every transform it later hands over
  // in that space. Eagerly for the same reason the seeds are.
  const SolidIntegral solid = integrateSolid(field);

  if (solid.mass > 0.0) {
    m_centerOfMass = glm::vec3(solid.firstMoment / double(solid.mass));

    // The parallel axis theorem, run backwards: the walk summed about the
    // field's origin, and a body turns about its centre of mass. An object
    // whose solid sits away from its own origin is otherwise handed an
    // inertia dominated by a distance that has nothing to do with its shape.
    const glm::dvec3 d(m_centerOfMass);
    const double m = solid.mass;

    glm::dmat3 aboutCentre = solid.secondMoment;

    aboutCentre[0][0] -= m * (d.y * d.y + d.z * d.z);
    aboutCentre[1][1] -= m * (d.x * d.x + d.z * d.z);
    aboutCentre[2][2] -= m * (d.x * d.x + d.y * d.y);
    aboutCentre[0][1] += m * d.x * d.y;
    aboutCentre[0][2] += m * d.x * d.z;
    aboutCentre[1][2] += m * d.y * d.z;

    aboutCentre[1][0] = aboutCentre[0][1];
    aboutCentre[2][0] = aboutCentre[0][2];
    aboutCentre[2][1] = aboutCentre[1][2];

    m_massProperties.mMass = static_cast<float>(solid.mass);
    m_massProperties.mInertia = JPH::Mat44::sIdentity();

    for (int column = 0; column != 3; ++column) {
      for (int row = 0; row != 3; ++row) {
        m_massProperties.mInertia(row, column) =
          static_cast<float>(aboutCentre[column][row]);
      }
    }
  }

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
  // Held about the centre of mass, which is the space Jolt reads it in.
  if (anySolid) {
    m_bounds = JPH::AABox(
      toJph(glm::max(solidLow, field.origin) - m_centerOfMass),
      toJph(glm::min(solidHigh, gridMaximum(field)) - m_centerOfMass)
    );

    return;
  }

  // Nothing solid anywhere, so there is no extent to be tighter than and the
  // grid is all this shape can answer for. Still about the centre of mass,
  // which is the origin here - written out so the two branches cannot drift.
  m_bounds = JPH::AABox(
    toJph(field.origin - m_centerOfMass),
    toJph(gridMaximum(field) - m_centerOfMass)
  );
}

const dunya::field::SampledField& FieldShape::field() const noexcept {
  return *m_field;
}

std::span<const FieldSeed> FieldShape::seeds() const noexcept {
  return m_seeds;
}

const glm::vec3& FieldShape::centerOfMass() const noexcept {
  return m_centerOfMass;
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
  return m_massProperties;
}

JPH::Vec3 FieldShape::GetCenterOfMass() const {
  return toJph(m_centerOfMass);
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
    dunya::field::probe(*m_field, toGlm(localSurfacePosition) + m_centerOfMass)
      .normal
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
  if (
    dunya::field::probe(*m_field, toGlm(point) + m_centerOfMass).distance
    >= 0.0f
  ) {
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
  // The candidates and the shape itself. Not the field, which is borrowed and
  // shared - counting it here bills every body built on it for the same 16 MiB.
  return Stats(sizeof(*this) + m_seeds.size() * sizeof(FieldSeed), 0u);
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
