#pragma once

#include "field/raycast.h"
#include "fieldpass/fieldpass.h"
#include "scene/scene.h"

#include <cstdint>

/* Changing the field at runtime.
 *
 * Takes a ray rather than a cursor: turning a mouse position into a world ray
 * needs the window and the camera, and neither of those is any of this class's
 * business. What it owns is what an edit *is* - where the cutter goes relative
 * to the surface, and getting the result to the GPU afterwards.
 */
class FieldEditor {
public:
  FieldEditor(Scene& scene, FieldPass& fieldPass);

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

private:
  Scene& m_scene;
  FieldPass& m_fieldPass;
};
