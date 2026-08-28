#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/editor/command/command.h>
#include <dunya/editor/commandhistory/commandhistory.h>
#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/world/world.h>

#include "tolerances.h"

#include <cstdint>

using Catch::Matchers::WithinAbs;

namespace {

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// slot from another after an edit has moved them around.
dunya::objectmodel::Entity makeObject(
  dunya::objectmodel::World& world,
  uint32_t primitives
) {
  dunya::objectmodel::FieldGrid object{};
  object.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  const dunya::objectmodel::Entity id =
    world.createField(dunya::objectmodel::Pose{}, object);

  for (uint32_t i = 0; i != primitives; ++i) {
    world.addPrimitive(
      id,
      dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, i + 1)
    );
  }

  return id;
}

uint32_t materialAt(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity id,
  uint32_t index
) {
  return world.primitives(id)[index].shapeConfig.y;
}

dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

const dunya::objectmodel::FieldGrid& gridOf(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
) {
  return world.registry().get<dunya::objectmodel::FieldGrid>(entity);
}

const dunya::objectmodel::Pose& poseOf(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
) {
  return world.registry().get<dunya::objectmodel::Pose>(entity);
}

}  // namespace

TEST_CASE(
  "undoing an added primitive restores the count and requeues the bake",
  "[commandhistory]"
) {
  // The bake only runs for objects the world flagged, so an undo that
  // forgets the flag changes the edit list and leaves the image behind.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 2);
  world.markBaked(id);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 2, marker(9)}, world)
  );

  REQUIRE(world.primitiveCount(id) == 3);
  REQUIRE(world.needsBake(id));

  world.markBaked(id);

  history.undo(world);

  REQUIRE(world.primitiveCount(id) == 2);
  REQUIRE(world.needsBake(id));
}

TEST_CASE(
  "a rejected edit returns false and records nothing",
  "[commandhistory]"
) {
  // An edit that never landed must stay out of the history: undoing it later
  // would remove a primitive this command never added.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  REQUIRE_FALSE(history.execute(
    dunya::editor::AddPrimitiveCommand{
      dunya::objectmodel::Entity{0},
      0,
      marker(1)
    },
    world
  ));

  REQUIRE_FALSE(history.canUndo());
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("redo replays an undone edit", "[commandhistory]") {
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 2);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 2, marker(9)}, world)
  );

  history.undo(world);

  REQUIRE(world.primitiveCount(id) == 2);
  REQUIRE(history.canRedo());

  history.redo(world);

  REQUIRE(world.primitiveCount(id) == 3);
  REQUIRE(materialAt(world, id, 2) == 9);
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("a fresh edit clears the redo stack", "[commandhistory]") {
  // Editing after an undo abandons the branch that was undone; keeping it
  // would let a later redo splice an edit onto a list it never saw.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 1);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 1, marker(9)}, world)
  );

  history.undo(world);

  REQUIRE(history.canRedo());

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 1, marker(8)}, world)
  );

  REQUIRE_FALSE(history.canRedo());
  REQUIRE(materialAt(world, id, 1) == 8);
}

TEST_CASE("undo runs last-in-first-out across objects", "[commandhistory]") {
  // Edit lists are per object but undo is global, so the history is the only
  // thing that knows which object was carved most recently.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity a = makeObject(world, 1);
  const dunya::objectmodel::Entity b = makeObject(world, 1);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{a, 1, marker(10)}, world)
  );
  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{b, 1, marker(20)}, world)
  );
  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{a, 2, marker(30)}, world)
  );

  REQUIRE(world.primitiveCount(a) == 3);
  REQUIRE(world.primitiveCount(b) == 2);

  history.undo(world);

  REQUIRE(world.primitiveCount(a) == 2);
  REQUIRE(world.primitiveCount(b) == 2);

  history.undo(world);

  REQUIRE(world.primitiveCount(a) == 2);
  REQUIRE(world.primitiveCount(b) == 1);

  history.undo(world);

  REQUIRE(world.primitiveCount(a) == 1);
  REQUIRE_FALSE(history.canUndo());
}

TEST_CASE(
  "undoing a removal puts the primitive back in its own slot",
  "[commandhistory]"
) {
  // Primitive order is the CSG fold order, so restoring at the end instead of
  // in place would rebuild a different shape from the same list.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 3);

  REQUIRE(history.execute(
    dunya::editor::RemovePrimitiveCommand{id, 1, world.primitives(id)[1]},
    world
  ));

  REQUIRE(world.primitiveCount(id) == 2);
  REQUIRE(materialAt(world, id, 0) == 1);
  REQUIRE(materialAt(world, id, 1) == 3);

  history.undo(world);

  REQUIRE(world.primitiveCount(id) == 3);
  REQUIRE(materialAt(world, id, 0) == 1);
  REQUIRE(materialAt(world, id, 1) == 2);
  REQUIRE(materialAt(world, id, 2) == 3);
}

TEST_CASE(
  "a redo that cannot apply stays on the redo stack",
  "[commandhistory]"
) {
  // A redo that fails must not consume the command: dropping it would lose
  // the edit from both stacks with nothing said.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 1);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 1, marker(9)}, world)
  );

  history.undo(world);

  world.destroyField(id);

  history.redo(world);

  REQUIRE(history.canRedo());
  REQUIRE_FALSE(history.canUndo());
}

TEST_CASE(
  "an undo that cannot revert stays on the undo stack",
  "[commandhistory]"
) {
  // The mirror of the failed redo. A revert that cannot run leaves the
  // history where it was rather than dropping the edit on the floor.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 1);

  REQUIRE(
    history.execute(dunya::editor::AddPrimitiveCommand{id, 1, marker(9)}, world)
  );

  world.destroyField(id);

  history.undo(world);

  REQUIRE(history.canUndo());
  REQUIRE_FALSE(history.canRedo());
}

TEST_CASE("undoing a transform restores the old pose", "[commandhistory]") {
  // A pose change never touches the edit list, so this one must move the
  // object back without asking for a rebake.
  dunya::objectmodel::World world;
  dunya::editor::CommandHistory history;

  const dunya::objectmodel::Entity id = makeObject(world, 1);

  const glm::vec3 oldPosition(1.0f, 2.0f, 3.0f);
  const glm::quat oldRotation =
    glm::angleAxis(glm::radians(15.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  const glm::vec3 newPosition(-4.0f, 0.0f, 0.5f);
  const glm::quat newRotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

  world.setPose(id, oldPosition, oldRotation);

  REQUIRE(history.execute(
    dunya::editor::TransformFieldCommand{
      id,
      oldPosition,
      oldRotation,
      newPosition,
      newRotation
    },
    world
  ));

  REQUIRE_THAT(
    poseOf(world, id).position.x,
    WithinAbs(newPosition.x, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    poseOf(world, id).rotation.w,
    WithinAbs(newRotation.w, ANALYTIC_TOLERANCE)
  );

  history.undo(world);

  REQUIRE_THAT(
    poseOf(world, id).position.x,
    WithinAbs(oldPosition.x, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    poseOf(world, id).position.z,
    WithinAbs(oldPosition.z, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    poseOf(world, id).rotation.w,
    WithinAbs(oldRotation.w, ANALYTIC_TOLERANCE)
  );
}
