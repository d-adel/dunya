#include "fieldeditor.ih"

FieldEditor::FieldEditor(Scene& scene, FieldPass& fieldPass)
    : m_scene(scene), m_fieldPass(fieldPass) {}

void FieldEditor::edit(uint32_t operation, const dunya::field::Ray& ray) {
  glm::vec3 origin =
    m_scene.fieldObjects().front().inverseModel() * glm::vec4(ray.origin, 1.0f);

  glm::vec3 direction = m_scene.fieldObjects().front().inverseModel()
                        * glm::vec4(ray.direction, 0.0f);

  dunya::field::Ray localRay{origin, direction};

  const std::optional<dunya::field::RayHit> hit =
    dunya::field::raymarch(m_scene.fieldObjects().front().editList, localRay);

  if (!hit.has_value()) {
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
                             ? hit->position - localRay.direction * offset
                             : hit->position + localRay.direction * offset;

  // What the CPU thinks is there before the edit. At the surface this should
  // read about zero, which is the agreement between the ray the CPU marched
  // and the surface the shader drew.
  const float surfaceDistance =
    dunya::field::sample(m_scene.primitives(), hit->position).distance;
  const float centreBefore =
    dunya::field::sample(m_scene.primitives(), centre).distance;

  if (!m_scene.addPrimitive(
        0,
        centre,
        EDIT_RADIUS,
        EDIT_BLEND,
        hit->material,
        operation
      )) {
    std::cout << "Primitive budget full, edit refused\n";
    return;
  }

  const auto uploadStart = std::chrono::steady_clock::now();
  m_fieldPass.primitivesChanged(m_scene.fieldObjects().front());
  const auto uploadEnd = std::chrono::steady_clock::now();

  // A carve leaves empty space at its centre and an add leaves solid, so both
  // land on +/- the edit radius. Anything else means the CPU and the GPU
  // disagree about the array they share.
  const float centreAfter =
    dunya::field::sample(m_scene.primitives(), centre).distance;

  const auto uploadMicros =
    std::chrono::duration_cast<std::chrono::microseconds>(
      uploadEnd - uploadStart
    )
      .count();

  std::cout << (fieldOpRemovesMaterial(operation) ? "carve" : "add  ")
            << "  surface " << std::fixed << std::setprecision(4)
            << surfaceDistance << "  centre " << std::setprecision(3)
            << centreBefore << " -> " << centreAfter << "  upload "
            << uploadMicros << " us  primitives " << m_scene.primitives().size()
            << '\n';
}

/* The placement is a low-discrepancy sequence rather than random: the carves
 * have to spread through the volume instead of stacking in a line, and they
 * have to land in the same places on every run, or the two representations are
 * not being measured on the same scene.
 */
void FieldEditor::stress(uint32_t count) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(m_scene.primitives());

  if (!extent.has_value()) {
    std::cout << "Nothing bounded to carve into\n";
    return;
  }

  const glm::vec3 span = extent->maximum - extent->minimum;
  const uint32_t before = static_cast<uint32_t>(m_scene.primitives().size());

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

  m_fieldPass.primitivesChanged(m_scene.fieldObjects().front());

  std::cout << "stress  primitives " << before << " -> "
            << m_scene.primitives().size() << '\n';
}
