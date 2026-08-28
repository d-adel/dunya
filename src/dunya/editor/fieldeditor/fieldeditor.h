#pragma once

#include <dunya/editor/commandhistory/commandhistory.h>
#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/world/world.h>

#include <cstdint>

// Changing the field at runtime. Takes a ray rather than a cursor, because
// turning a mouse position into one needs the window and the camera.

namespace dunya::editor {

class FieldEditor {
public:
  explicit FieldEditor(dunya::objectmodel::World& world);

  FieldEditor(const FieldEditor&) = delete;
  FieldEditor& operator=(const FieldEditor&) = delete;
  FieldEditor(FieldEditor&&) = delete;
  FieldEditor& operator=(FieldEditor&&) = delete;

  ~FieldEditor() = default;

  // Carves or adds where this ray meets the surface. Does nothing when it
  // meets nothing.
  void edit(uint32_t operation, const dunya::field::Ray& ray);

  // Carves a batch at fixed positions, so a measurement can reach a primitive
  // count that clicking cannot reach patiently or repeatably.
  void stress(uint32_t count);

  // Undo and redo live here rather than on the world: a history is what an
  // editor remembers doing to a world, not something the world knows.
  void undo();
  void redo();

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

  dunya::objectmodel::World& m_world;
  CommandHistory m_commandHistory;
};

}  // namespace dunya::editor
