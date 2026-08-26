#include "fieldeditor.ih"

FieldEditor::FieldEditor(Scene& scene) : m_scene(scene) {}

void FieldEditor::edit(uint32_t operation, const dunya::field::Ray& ray) {
  int objectIndex = -1;
  dunya::field::RayHit minHit;
  dunya::field::Ray localRay;

  const ObjectRegistry& registry = m_scene.registry();
  for (ObjectId id : registry.fieldObjectIds()) {
    const auto& fieldObject = registry.getFieldObject(id);
    const auto primitives = registry.getPrimitives(id);

    glm::mat4 inverseModel = glm::inverse(fieldObject.model());

    glm::vec3 origin = inverseModel * glm::vec4(ray.origin, 1.0f);

    glm::vec3 direction = inverseModel * glm::vec4(ray.direction, 0.0f);

    dunya::field::Ray curRay = {origin, direction};

    std::optional<std::pair<float, float>> tOpt(std::nullopt);
    dunya::field::Aabb box = gridBox(primitives);
    tOpt = dunya::field::intersect(box, curRay);

    if (tOpt.has_value()) {
      std::optional<dunya::field::RayHit> hit =
        dunya::field::raymarch(primitives, curRay);
      if (hit && (objectIndex == -1 || minHit.travelled > hit->travelled)) {
        minHit = hit.value();
        objectIndex = static_cast<int>(id);
        localRay = curRay;
      }
    }
  }

  if (objectIndex == -1) {
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
  const float offset = EDIT_RADIUS - EDIT_ADVANCE;
  const glm::vec3 centre = fieldOpRemovesMaterial(operation)
                             ? minHit.position - localRay.direction * offset
                             : minHit.position + localRay.direction * offset;

  if (!m_scene.addPrimitive(
        objectIndex,
        centre,
        EDIT_RADIUS,
        EDIT_BLEND,
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
  const ObjectRegistry& registry = m_scene.registry();
  auto primitives = registry.getPrimitives(0);

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
    if (!m_scene.addPrimitive(
          0,
          extent->minimum + span * at,
          EDIT_RADIUS,
          0.0f,
          0,
          FIELD_OP_SUBTRACTION
        )) {
      std::cout << "Primitive budget full\n";
      break;
    }
  }

  primitives = registry.getPrimitives(0);
  m_scene.setDirty(0, true);
  std::cout << "stress  primitives " << before << " -> " << primitives.size()
            << '\n';
}
