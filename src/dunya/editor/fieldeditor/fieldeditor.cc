#include "fieldeditor.ih"

namespace dunya::editor {

FieldEditor::FieldEditor(dunya::objectmodel::World& world) : m_world(world) {}

void FieldEditor::edit(uint32_t operation, const dunya::field::Ray& ray) {
  dunya::objectmodel::Entity hitEntity = dunya::objectmodel::INVALID_ENTITY;

  dunya::field::RayHit minHit;
  dunya::field::Ray localRay;
  const entt::registry& registry = m_world.registry();

  for (dunya::objectmodel::Entity entity : m_world.fields()) {
    std::span<const dunya::field::Primitive> primitives =
      m_world.primitives(entity);

    glm::mat4 inverseModel = glm::inverse(
      dunya::objectmodel::model(registry.get<dunya::objectmodel::Pose>(entity))
    );

    glm::vec3 origin = inverseModel * glm::vec4(ray.origin, 1.0f);

    glm::vec3 direction = inverseModel * glm::vec4(ray.direction, 0.0f);

    dunya::field::Ray curRay{origin, direction};

    const dunya::field::Aabb box = dunya::objectmodel::gridBox(primitives);

    const auto tOpt = dunya::field::intersect(box, curRay);

    if (!tOpt) {
      continue;
    }

    const auto hit = dunya::field::raymarch(primitives, curRay);

    if (
    hit
    && (
      hitEntity == dunya::objectmodel::INVALID_ENTITY
      || hit->travelled < minHit.travelled
    )
  ) {
      minHit = *hit;
      hitEntity = entity;
      localRay = curRay;
    }
  }

  if (hitEntity == dunya::objectmodel::INVALID_ENTITY) {
    std::cout << "Nothing under the cursor\n";
    return;
  }

  /* Place the sphere so that its far wall lands exactly one advance past the
   * surface, which is what makes a click move the surface by EDIT_ADVANCE.
   *
   * For a carve that means pulling it back out of the material toward the eye;
   * for an add, pushing it in. Same distance, opposite sign, because a carve
   * moves the surface away from the eye and an add moves it toward.
   *
   * The offset is bounded by the radius at both ends, and both ends are
   * degenerate. Push a carve a full radius in and the clicked point lands
   * exactly *on* the cutter, so its distance there is zero, so max(acc, -0)
   * leaves the field untouched: the surface does not move where it was aimed,
   * the next march finds the same point, and clicking repeatedly appends
   * identical primitives and does nothing. Pull it a full radius out and the
   * cutter no longer reaches the surface at all. Everything useful is strictly
   * between, and which point in between is EDIT_ADVANCE's decision.
   */
  const float offset = dunya::core::EDIT_RADIUS - dunya::core::EDIT_ADVANCE;
  const glm::vec3 centre = dunya::core::fieldOpRemovesMaterial(operation)
                             ? minHit.position - localRay.direction * offset
                             : minHit.position + localRay.direction * offset;

  if (!addPrimitive(
        hitEntity,
        centre,
        dunya::core::EDIT_RADIUS,
        dunya::core::EDIT_BLEND,
        minHit.material,
        operation
      )) {
    std::cout << "Primitive budget full, edit refused\n";
    return;
  }
}

/* The placement is a low-discrepancy sequence rather than random: the carves
 * have to spread through the volume instead of stacking in a line, and they
 * have to land in the same places on every run, or the two representations are
 * not being measured on the same scene.
 */
void FieldEditor::stress(uint32_t count) {
  if (m_world.fields().empty()) {
    std::cout << "No field to carve into\n";
    return;
  }

  const dunya::objectmodel::Entity target = m_world.fields()[0];

  std::span<const dunya::field::Primitive> primitives =
    m_world.primitives(target);

  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  if (!extent.has_value()) {
    std::cout << "Nothing bounded to carve into\n";
    return;
  }

  const glm::vec3 span = extent->maximum - extent->minimum;
  const uint32_t before = static_cast<uint32_t>(primitives.size());

  for (uint32_t i = 0; i < count; ++i) {
    // The R3 sequence: successive multiples of these three fractions fill a
    // volume evenly without ever repeating a position.
    const glm::vec3 at = glm::fract(
      static_cast<float>(before + i)
      * glm::vec3(0.8191725f, 0.6710436f, 0.5497005f)
    );

    // Deliberately the hard op with no blend, unlike a click. M17's comparison
    // table was measured with this, and a smooth carve costs an extra smin per
    // primitive per sample, so quietly changing it here would move published
    // numbers without saying so.
    if (!addPrimitive(
          target,
          extent->minimum + span * at,
          dunya::core::EDIT_RADIUS,
          0.0f,
          0,
          dunya::core::FIELD_OP_SUBTRACTION
        )) {
      std::cout << "Primitive budget full\n";
      break;
    }
  }

  primitives = m_world.primitives(target);
  std::cout << "stress  primitives " << before << " -> " << primitives.size()
            << '\n';
}

bool FieldEditor::addPrimitive(
  dunya::objectmodel::Entity entity,
  const glm::vec3& centre,
  float radius,
  float blend,
  uint32_t material,
  uint32_t operation
) {
  if (
    !m_world.registry().valid(entity)
    || !m_world.registry().all_of<dunya::objectmodel::FieldGrid>(entity)
  ) {
    return false;
  }

  const dunya::field::Primitive primitive =
    dunya::field::makeSphere(centre, radius, material, operation, blend);

  AddPrimitiveCommand command(
    entity,
    m_world.primitiveCount(entity),
    primitive
  );

  return m_commandHistory.execute(command, m_world);
}

void FieldEditor::undo() {
  m_commandHistory.undo(m_world);
}

void FieldEditor::redo() {
  m_commandHistory.redo(m_world);
}

}  // namespace dunya::editor
