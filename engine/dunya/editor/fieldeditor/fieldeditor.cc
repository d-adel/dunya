#include "fieldeditor.ih"

namespace dunya::editor {

FieldEditor::FieldEditor(dunya::objectmodel::World& world) : m_world(&world) {}

void FieldEditor::edit(uint32_t operation, const dunya::field::Ray& ray) {
  dunya::objectmodel::Entity hitEntity = dunya::objectmodel::INVALID_ENTITY;

  dunya::field::RayHit minHit;
  dunya::field::Ray localRay;
  const entt::registry& registry = m_world->registry();

  for (dunya::objectmodel::Entity entity : m_world->fields()) {
    std::span<const dunya::field::Primitive> primitives =
      m_world->primitives(entity);

    glm::mat4 inverseModel = glm::inverse(
      dunya::objectmodel::model(registry.get<dunya::objectmodel::Pose>(entity))
    );

    glm::vec3 origin = inverseModel * glm::vec4(ray.origin, 1.0f);

    glm::vec3 direction = inverseModel * glm::vec4(ray.direction, 0.0f);

    dunya::field::Ray curRay{origin, direction};

    const dunya::field::Aabb box = dunya::objectmodel::gridBox(
      primitives,
      dunya::objectmodel::gridMargin(
        registry.get<dunya::objectmodel::SdfGrid>(entity),
        primitives
      )
    );

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

void FieldEditor::stress(uint32_t count) {
  if (m_world->fields().empty()) {
    std::cout << "No field to carve into\n";
    return;
  }

  const dunya::objectmodel::Entity target = m_world->fields()[0];

  std::span<const dunya::field::Primitive> primitives =
    m_world->primitives(target);

  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  if (!extent.has_value()) {
    std::cout << "Nothing bounded to carve into\n";
    return;
  }

  const glm::vec3 span = extent->maximum - extent->minimum;
  const uint32_t before = static_cast<uint32_t>(primitives.size());

  for (uint32_t i = 0; i < count; ++i) {
    const glm::vec3 at = glm::fract(
      static_cast<float>(before + i)
      * glm::vec3(0.8191725f, 0.6710436f, 0.5497005f)
    );

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

  primitives = m_world->primitives(target);
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
    !m_world->registry().valid(entity)
    || !m_world->registry().all_of<dunya::objectmodel::SdfGrid>(entity)
  ) {
    return false;
  }

  const dunya::field::Primitive primitive =
    dunya::field::makeSphere(centre, radius, material, operation, blend);

  AddPrimitiveCommand command(
    entity,
    m_world->primitiveCount(entity),
    primitive
  );

  return m_commandHistory.execute(command, *m_world);
}

void FieldEditor::undo() {
  m_commandHistory.undo(*m_world);
}

void FieldEditor::redo() {
  m_commandHistory.redo(*m_world);
}

}
