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

  // The collision shape has to follow the crater, or the next ball rolls over a
  // hole it should fall into.
  runtime.reshapeAfterDeform(entity, report.brickBegin, report.brickEnd);

  // And whatever was asleep on top of it has to be told, because Jolt only
  // invalidates the contact cache of the body whose shape it just swapped. In
  // world space, so the pose the cutter was expressed against is undone.
  const glm::mat4 model =
    objectmodel::model(world.registry().get<objectmodel::Pose>(entity));

  const glm::vec3 centre(cutter.bounds);
  const glm::vec3 reach(cutter.bounds.w * 2.0f);

  const glm::vec3 at = model * glm::vec4(centre, 1.0f);

  runtime.wake(at - reach, at + reach);

  markDirty(entity, report.samples);
}

void Deformation::applyImpacts(Runtime& runtime) {
  m_carved.clear();

  runtime.physics().impacts().drain(m_impacts);

  const entt::registry& registry = runtime.world().registry();

  for (const physics::Impact& impact : m_impacts) {
    const objectmodel::Entity entity{impact.entity};

    // Both sides of every manifold arrive, and most of them are not
    // deformable: the ball that threw the punch, and the ground before it is
    // tagged. Nothing to do for those.
    if (
      !registry.valid(entity)
      || !registry.all_of<objectmodel::Deformable, objectmodel::SharedField>(
        entity
      )
    ) {
      continue;
    }

    // Into the field's own frame here rather than at the carve, which is the
    // point of deferring at all: the body keeps moving, and a world-space
    // contact recorded this frame describes nowhere in particular by the time a
    // later frame gets to it. The same crossing FieldEditor::edit makes for a
    // click.
    const glm::mat4 inverseModel =
      glm::inverse(objectmodel::model(registry.get<objectmodel::Pose>(entity)));

    m_pending.push_back(
      {entity,
       glm::vec3(inverseModel * glm::vec4(impact.point, 1.0f)),

       // A direction, so the translation is dropped. The pose is rigid, so this
       // stays unit length and the normalize is belt and braces.
       glm::normalize(
         glm::vec3(inverseModel * glm::vec4(impact.outward, 0.0f))
       ),
       impact.impulse}
    );
  }

  if (m_pending.empty()) {
    return;
  }

  // Hardest first, so a frame that cannot afford all of them spends what it has
  // on the ones that show. Ties break on the entity, which costs nothing and
  // makes the order total: std::sort is not stable, and a demo that has to
  // reproduce should not depend on which of two equal hits it picked.
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

    // A frame or more may have passed, and the wall has been falling over in
    // the meantime. An entity that has gone takes its craters with it.
    if (
      !registry.valid(entity)
      || !registry.all_of<objectmodel::Deformable, objectmodel::SharedField>(
        entity
      )
    ) {
      continue;
    }

    // What the object can afford to lose. The primitives rather than the grid,
    // because the grid carries a margin that has nothing to do with how big the
    // object is - and the shortest side is the one that runs out first: a floor
    // is thin and wide, and it is the thickness a crater has to respect. D5
    // leaves the primitive list describing the object as authored, which is the
    // right thing to measure damage against.
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

    // Recovered from the radius rather than kept, so a crater the object's size
    // capped stays a cap of the right shape instead of a deep puncture in a
    // narrow sphere.
    const float depth = radius / m_damage.radiusPerDepth;

    // Sunk along the inward normal so the sphere's near cap sits at the surface
    // and exactly `depth` of it is inside: the centre goes back by the rest of
    // the radius.
    const glm::vec3 centre = pending.point - pending.outward * (radius - depth);

    field::Primitive cutter =
      field::makeSphere(centre, radius, 1u, core::FIELD_OP_SUBTRACTION);

    field::updateBounds(cutter);

    const auto started = std::chrono::steady_clock::now();

    carve(runtime, entity, cutter);
    ++m_cratersApplied;

    m_carved.push_back(
      {entity,
       pending.impulse,
       depth,
       radius,
       std::chrono::duration<float, std::milli>(
         std::chrono::steady_clock::now() - started
       )
         .count()}
    );
  }

  // Everything the budget could not reach is dropped rather than carried: a
  // ball that has already punched through has moved on, and a crater that
  // arrives a second later reads as a glitch rather than as damage. The sort
  // above means what goes is always the weakest.
  m_pending.clear();
}

}  // namespace dunya::runtime
