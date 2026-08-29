#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::physics {

// A contact candidate: one per brick that holds surface, pulled onto the zero
// set. Where the brick is decides which contact this is, so a manifold keyed
// on it stays keyed on the same thing between frames.
struct FieldSeed {
  glm::vec3 point;
  uint32_t brick;
};

// A Jolt collision shape over a sampled field. Contacts come from the field
// itself - distance and normal at a point - so there is no mesh, no convex
// decomposition, and no restriction on the geometry's shape.
//
// It borrows the field rather than owning it: at 16 MiB a copy per body is not
// affordable. The field must therefore outlive every body built on it, and a
// rebake replaces it in place, so a rebake must rebuild the shape as well.
class FieldShape final : public JPH::Shape {
public:
  explicit FieldShape(const dunya::field::SampledField& field);

  const dunya::field::SampledField& field() const noexcept;

  // The candidates, in brick order. Pulling a brick centre onto the surface is
  // a pure function of the field, and the alternative is solving it again per
  // pair, per substep, and once per iteration of every sweep.
  std::span<const FieldSeed> seeds() const noexcept;

  JPH::AABox GetLocalBounds() const override;
  JPH::uint GetSubShapeIDBitsRecursive() const override;
  float GetInnerRadius() const override;
  JPH::MassProperties GetMassProperties() const override;

  const JPH::PhysicsMaterial* GetMaterial(
    const JPH::SubShapeID& subShapeID
  ) const override;

  JPH::Vec3 GetSurfaceNormal(
    const JPH::SubShapeID& subShapeID,
    JPH::Vec3Arg localSurfacePosition
  ) const override;

  void GetSubmergedVolume(
    JPH::Mat44Arg centerOfMassTransform,
    JPH::Vec3Arg scale,
    const JPH::Plane& surface,
    float& totalVolume,
    float& submergedVolume,
    JPH::Vec3& centerOfBuoyancy
#ifdef JPH_DEBUG_RENDERER
    ,
    JPH::RVec3Arg baseOffset
#endif
  ) const override;

#ifdef JPH_DEBUG_RENDERER
  void Draw(
    JPH::DebugRenderer* renderer,
    JPH::RMat44Arg centerOfMassTransform,
    JPH::Vec3Arg scale,
    JPH::ColorArg color,
    bool useMaterialColors,
    bool drawWireframe
  ) const override;
#endif

  bool CastRay(
    const JPH::RayCast& ray,
    const JPH::SubShapeIDCreator& subShapeIDCreator,
    JPH::RayCastResult& hit
  ) const override;

  void CastRay(
    const JPH::RayCast& ray,
    const JPH::RayCastSettings& rayCastSettings,
    const JPH::SubShapeIDCreator& subShapeIDCreator,
    JPH::CastRayCollector& collector,
    const JPH::ShapeFilter& shapeFilter
  ) const override;

  void CollidePoint(
    JPH::Vec3Arg point,
    const JPH::SubShapeIDCreator& subShapeIDCreator,
    JPH::CollidePointCollector& collector,
    const JPH::ShapeFilter& shapeFilter
  ) const override;

  void CollideSoftBodyVertices(
    JPH::Mat44Arg centerOfMassTransform,
    JPH::Vec3Arg scale,
    const JPH::CollideSoftBodyVertexIterator& vertices,
    JPH::uint numVertices,
    int collidingShapeIndex
  ) const override;

  void GetTrianglesStart(
    GetTrianglesContext& context,
    const JPH::AABox& box,
    JPH::Vec3Arg positionCOM,
    JPH::QuatArg rotation,
    JPH::Vec3Arg scale
  ) const override;

  int GetTrianglesNext(
    GetTrianglesContext& context,
    int maxTrianglesRequested,
    JPH::Float3* triangleVertices,
    const JPH::PhysicsMaterial** materials
  ) const override;

  Stats GetStats() const override;
  float GetVolume() const override;

private:
  const dunya::field::SampledField* m_field;

  std::vector<FieldSeed> m_seeds;

  // A shape is immutable, so the walk over two million cells is a pure
  // function of the field it borrows: done once, on the first body built on
  // it, and kept. Jolt asks at body creation and at SetShape, never per step.
  mutable JPH::MassProperties m_massProperties;
  mutable bool m_massKnown = false;

  JPH::AABox m_bounds;
  float m_innerRadius = 0.0f;
  JPH::uint m_subShapeBits = 0u;
};

// Fills the dispatch table for field-versus-field collision and sweeps. Must
// run after JPH::RegisterTypes(), which unconditionally overwrites every
// User slot on behalf of the decorator shapes.
void registerFieldShape();

}  // namespace dunya::physics
