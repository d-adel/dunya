#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "command/command.h"
#include "commandhistory/commandhistory.h"
#include "config/config.h"
#include "field/field.h"
#include "objectregistry/objectregistry.h"

#include "tolerances.h"

#include <cstdint>

using Catch::Matchers::WithinAbs;

namespace {

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// slot from another after an edit has moved them around.
ObjectId makeObject(ObjectRegistry& registry, uint32_t primitives) {
  FieldObject object{};
  object.resolution = glm::uvec3(FIELD_GRID_RESOLUTION);

  const ObjectId id = registry.addFieldObject(object);

  for (uint32_t i = 0; i != primitives; ++i) {
    registry.addPrimitive(
      id,
      dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, i + 1)
    );
  }

  return id;
}

uint32_t materialAt(
  const ObjectRegistry& registry,
  ObjectId id,
  uint32_t index
) {
  return registry.getPrimitives(id)[index].shapeConfig.y;
}

dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

}  // namespace

TEST_CASE("undoing an added primitive restores the count and marks it dirty",
          "[commandhistory]") {
  // The bake only runs for objects the registry flagged, so an undo that
  // forgets the flag changes the edit list and leaves the image behind.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 2);
  registry.getFieldObject(id).dirty = false;

  REQUIRE(history.execute(AddPrimitiveCommand{id, 2, marker(9)}, registry));

  REQUIRE(registry.primitiveCount(id) == 3);
  REQUIRE(registry.getFieldObject(id).dirty);

  registry.getFieldObject(id).dirty = false;

  history.undo(registry);

  REQUIRE(registry.primitiveCount(id) == 2);
  REQUIRE(registry.getFieldObject(id).dirty);
}

TEST_CASE("a rejected edit returns false and records nothing",
          "[commandhistory]") {
  // An edit that never landed must stay out of the history: undoing it later
  // would remove a primitive this command never added.
  ObjectRegistry registry;
  CommandHistory history;

  REQUIRE_FALSE(
    history.execute(AddPrimitiveCommand{0, 0, marker(1)}, registry)
  );

  REQUIRE_FALSE(history.canUndo());
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("redo replays an undone edit", "[commandhistory]") {
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 2);

  REQUIRE(history.execute(AddPrimitiveCommand{id, 2, marker(9)}, registry));

  history.undo(registry);

  REQUIRE(registry.primitiveCount(id) == 2);
  REQUIRE(history.canRedo());

  history.redo(registry);

  REQUIRE(registry.primitiveCount(id) == 3);
  REQUIRE(materialAt(registry, id, 2) == 9);
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("a fresh edit clears the redo stack", "[commandhistory]") {
  // Editing after an undo abandons the branch that was undone; keeping it
  // would let a later redo splice an edit onto a list it never saw.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 1);

  REQUIRE(history.execute(AddPrimitiveCommand{id, 1, marker(9)}, registry));

  history.undo(registry);

  REQUIRE(history.canRedo());

  REQUIRE(history.execute(AddPrimitiveCommand{id, 1, marker(8)}, registry));

  REQUIRE_FALSE(history.canRedo());
  REQUIRE(materialAt(registry, id, 1) == 8);
}

TEST_CASE("undo runs last-in-first-out across objects", "[commandhistory]") {
  // Edit lists are per object but undo is global, so the history is the only
  // thing that knows which object was carved most recently.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId a = makeObject(registry, 1);
  const ObjectId b = makeObject(registry, 1);

  REQUIRE(history.execute(AddPrimitiveCommand{a, 1, marker(10)}, registry));
  REQUIRE(history.execute(AddPrimitiveCommand{b, 1, marker(20)}, registry));
  REQUIRE(history.execute(AddPrimitiveCommand{a, 2, marker(30)}, registry));

  REQUIRE(registry.primitiveCount(a) == 3);
  REQUIRE(registry.primitiveCount(b) == 2);

  history.undo(registry);

  REQUIRE(registry.primitiveCount(a) == 2);
  REQUIRE(registry.primitiveCount(b) == 2);

  history.undo(registry);

  REQUIRE(registry.primitiveCount(a) == 2);
  REQUIRE(registry.primitiveCount(b) == 1);

  history.undo(registry);

  REQUIRE(registry.primitiveCount(a) == 1);
  REQUIRE_FALSE(history.canUndo());
}

TEST_CASE("undoing a removal puts the primitive back in its own slot",
          "[commandhistory]") {
  // Primitive order is the CSG fold order, so restoring at the end instead of
  // in place would rebuild a different shape from the same list.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 3);

  REQUIRE(history.execute(
    RemovePrimitiveCommand{id, 1, registry.getPrimitives(id)[1]},
    registry
  ));

  REQUIRE(registry.primitiveCount(id) == 2);
  REQUIRE(materialAt(registry, id, 0) == 1);
  REQUIRE(materialAt(registry, id, 1) == 3);

  history.undo(registry);

  REQUIRE(registry.primitiveCount(id) == 3);
  REQUIRE(materialAt(registry, id, 0) == 1);
  REQUIRE(materialAt(registry, id, 1) == 2);
  REQUIRE(materialAt(registry, id, 2) == 3);
}

TEST_CASE("a redo that cannot apply stays on the redo stack",
          "[commandhistory]") {
  // A redo that fails must not consume the command: dropping it would lose
  // the edit from both stacks with nothing said.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 1);

  REQUIRE(history.execute(AddPrimitiveCommand{id, 1, marker(9)}, registry));

  history.undo(registry);

  registry.removeFieldObject(id);

  history.redo(registry);

  REQUIRE(history.canRedo());
  REQUIRE_FALSE(history.canUndo());
}

TEST_CASE("an undo that cannot revert stays on the undo stack",
          "[commandhistory]") {
  // The mirror of the failed redo. A revert that cannot run leaves the
  // history where it was rather than dropping the edit on the floor.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 1);

  REQUIRE(history.execute(AddPrimitiveCommand{id, 1, marker(9)}, registry));

  registry.removeFieldObject(id);

  history.undo(registry);

  REQUIRE(history.canUndo());
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("undoing a transform restores the old pose", "[commandhistory]") {
  // A pose change never touches the edit list, so this one must move the
  // object back without asking for a rebake.
  ObjectRegistry registry;
  CommandHistory history;

  const ObjectId id = makeObject(registry, 1);

  const glm::vec3 oldPosition(1.0f, 2.0f, 3.0f);
  const glm::quat oldRotation =
    glm::angleAxis(glm::radians(15.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  const glm::vec3 newPosition(-4.0f, 0.0f, 0.5f);
  const glm::quat newRotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

  FieldObject& object = registry.getFieldObject(id);
  object.position = oldPosition;
  object.rotation = oldRotation;

  REQUIRE(history.execute(
    TransformFieldObjectCommand{
      id,
      oldPosition,
      oldRotation,
      newPosition,
      newRotation
    },
    registry
  ));

  REQUIRE_THAT(object.position.x, WithinAbs(newPosition.x, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(object.rotation.w, WithinAbs(newRotation.w, ANALYTIC_TOLERANCE));

  history.undo(registry);

  REQUIRE_THAT(object.position.x, WithinAbs(oldPosition.x, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(object.position.z, WithinAbs(oldPosition.z, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(object.rotation.w, WithinAbs(oldRotation.w, ANALYTIC_TOLERANCE));
}
