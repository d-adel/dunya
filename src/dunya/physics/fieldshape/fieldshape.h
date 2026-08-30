#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::physics {

struct FieldSeed {
  glm::vec3 point;
  uint32_t brick;
};

struct SolidIntegral {
  double mass = 0.0;
  glm::dvec3 firstMoment{0.0};
  glm::dmat3 secondMoment{0.0};
};

class FieldShape final : public JPH::Shape {
public:
  explicit FieldShape(const dunya::field::SampledField& field);

  FieldShape(
    const dunya::field::SampledField& field,
    const FieldShape& previous,
    const glm::uvec3& changedBegin,
    const glm::uvec3& changedEnd
  );

  const dunya::field::SampledField& field() const noexcept;

  const glm::vec3& centerOfMass() const noexcept;

  std::span<const FieldSeed> seeds() const noexcept;

  JPH::AABox GetLocalBounds() const override;
  JPH::Vec3 GetCenterOfMass() const override;
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
  FieldShape(
    const dunya::field::SampledField& field,
    const FieldShape* previous,
    const glm::uvec3& changedBegin,
    const glm::uvec3& changedEnd
  );

  const dunya::field::SampledField* m_field;

  std::vector<SolidIntegral> m_brickIntegral;
  std::vector<FieldSeed> m_brickSeed;

  std::vector<FieldSeed> m_seeds;

  glm::vec3 m_centerOfMass{0.0f};
  JPH::MassProperties m_massProperties;

  JPH::AABox m_bounds;
  float m_innerRadius = 0.0f;
  JPH::uint m_subShapeBits = 0u;
};

void registerFieldShape();

}
