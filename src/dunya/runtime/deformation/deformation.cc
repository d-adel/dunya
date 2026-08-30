#include "deformation.ih"

namespace dunya::runtime {

Deformation::Damage& Deformation::damage() noexcept {
  return m_damage;
}

const Deformation::Damage& Deformation::damage() const noexcept {
  return m_damage;
}

uint32_t Deformation::cratersApplied() const noexcept {
  return m_cratersApplied;
}

void Deformation::markDirty(
  objectmodel::Entity entity,
  const field::SampleBox& box
) {
  const auto found =
    std::find_if(m_dirty.begin(), m_dirty.end(), [entity](const auto& entry) {
      return entry.first == entity;
    });

  if (found == m_dirty.end()) {
    m_dirty.emplace_back(entity, box);
  } else {
    found->second = field::merge(found->second, box);
  }
}

std::span<const std::pair<objectmodel::Entity, field::SampleBox>> Deformation::
  dirty() const noexcept {
  return m_dirty;
}

void Deformation::clearDirty() noexcept {
  m_dirty.clear();
}

std::span<const Deformation::Crater> Deformation::
  cratersThisFrame() const noexcept {
  return m_carved;
}

void Deformation::carve(
  Runtime& runtime,
  objectmodel::Entity entity,
  const field::Primitive& cutter
) {
  objectmodel::World& world = runtime.world();

  field::WriteReport report{};

  world.patchSampledField(entity, [&](field::SampledField& field) {
    report = field::deformAndRepair(field, cutter).write;
  });

  runtime.reshapeAfterDeform(entity, report.brickBegin, report.brickEnd);

  const glm::mat4 model =
    objectmodel::model(world.registry().get<objectmodel::Pose>(entity));

  const glm::vec3 centre(cutter.bounds);
  const glm::vec3 reach(cutter.bounds.w * 2.0f);

  const glm::vec3 at = model * glm::vec4(centre, 1.0f);

  runtime.wake(at - reach, at + reach);

  markDirty(entity, report.samples);
}

void Deformation::applyImpacts(Runtime& runtime, core::Telemetry& telemetry) {
  const auto craters = telemetry.key("craters");
  m_carved.clear();

  runtime.physics().impacts().drain(m_impacts);

  const entt::registry& registry = runtime.world().registry();

  for (const physics::Impact& impact : m_impacts) {
    const objectmodel::Entity entity{impact.entity};

    if (
      !registry.valid(entity)
      || !registry.all_of<objectmodel::Deformable, objectmodel::SharedField>(
        entity
      )
    ) {
      continue;
    }

    const glm::mat4 inverseModel =
      glm::inverse(objectmodel::model(registry.get<objectmodel::Pose>(entity)));

    m_pending.push_back(
      {entity,
       glm::vec3(inverseModel * glm::vec4(impact.point, 1.0f)),

       glm::normalize(
         glm::vec3(inverseModel * glm::vec4(impact.outward, 0.0f))
       ),
       impact.impulse}
    );
  }

  if (m_pending.empty()) {
    return;
  }

  std::sort(
    m_pending.begin(),
    m_pending.end(),
    [](const PendingCrater& a, const PendingCrater& b) {
      if (a.impulse != b.impulse) {
        return a.impulse > b.impulse;
      }

      return static_cast<uint32_t>(a.entity) < static_cast<uint32_t>(b.entity);
    }
  );

  const size_t budget = std::min<size_t>(m_damage.perFrame, m_pending.size());

  for (size_t i = 0; i != budget; ++i) {
    const PendingCrater& pending = m_pending[i];
    const objectmodel::Entity entity = pending.entity;

    if (
      !registry.valid(entity)
      || !registry.all_of<objectmodel::Deformable, objectmodel::SharedField>(
        entity
      )
    ) {
      continue;
    }

    const std::optional<field::Aabb> extent =
      field::boundedExtent(runtime.world().primitives(entity));

    const glm::vec3 span =
      extent.has_value() ? extent->maximum - extent->minimum : glm::vec3(1.0f);

    const float widest =
      m_damage.widestFraction * std::min({span.x, span.y, span.z});

    const float radius = std::min(
      m_damage.radiusPerDepth
        * std::clamp(
          m_damage.depthPerImpulse * pending.impulse,
          m_damage.minimumDepth,
          m_damage.maximumDepth
        ),
      widest
    );

    const float depth = radius / m_damage.radiusPerDepth;

    const glm::vec3 centre = pending.point - pending.outward * (radius - depth);

    field::Primitive cutter =
      field::makeSphere(centre, radius, 1u, core::FIELD_OP_SUBTRACTION);

    field::updateBounds(cutter);

    carve(runtime, entity, cutter);
    ++m_cratersApplied;

    telemetry.add(craters, 1.0);

    m_carved.push_back({entity, pending.impulse, depth, radius});
  }

  m_pending.clear();
}

}
