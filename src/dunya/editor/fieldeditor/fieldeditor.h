#pragma once

#include <dunya/editor/commandhistory/commandhistory.h>
#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/world/world.h>

#include <cstdint>

namespace dunya::editor {

class FieldEditor {
public:
  explicit FieldEditor(dunya::objectmodel::World& world);

  FieldEditor(const FieldEditor&) = delete;
  FieldEditor& operator=(const FieldEditor&) = delete;
  FieldEditor(FieldEditor&&) = delete;
  FieldEditor& operator=(FieldEditor&&) = delete;

  ~FieldEditor() = default;

  void edit(uint32_t operation, const dunya::field::Ray& ray);

  void stress(uint32_t count);

  void undo();
  void redo();

  void retarget(dunya::objectmodel::World& world);

private:
  [[nodiscard]]
  bool addPrimitive(
    dunya::objectmodel::Entity entity,
    const glm::vec3& centre,
    float radius,
    float blend,
    uint32_t material,
    uint32_t operation
  );

  dunya::objectmodel::World* m_world;
  CommandHistory m_commandHistory;
};

}
