#include <catch2/catch_test_macros.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/deformed/deformed.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldcontents/worldcontents.h>
#include <dunya/undo/undostack/undostack.h>

#include <cstdint>
#include <utility>

namespace {

using dunya::objectmodel::Deformable;
using dunya::objectmodel::Deformed;
using dunya::objectmodel::Entity;
using dunya::objectmodel::MassScale;
using dunya::objectmodel::Material;
using dunya::objectmodel::Pose;
using dunya::objectmodel::SdfGrid;
using dunya::objectmodel::StaticBody;
using dunya::objectmodel::World;
using dunya::undo::UndoStack;

SdfGrid blank() {
  SdfGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  return grid;
}

Pose marked(float marker) {
  Pose pose{};
  pose.position.x = marker;

  return pose;
}

float markerAt(const World& world, Entity entity) {
  return world.registry().get<Pose>(entity).position.x;
}

void dent(World& world, Entity entity) {
  world.emplaceOrReplace<Deformable>(entity, Deformable{});

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(entity, std::move(field));

  world.patchSampledSdf(entity, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -1.0f;
  });
}

}

TEST_CASE("an empty stack has nothing to undo", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  REQUIRE_FALSE(stack.undo(world));
  REQUIRE_FALSE(stack.redo(world));
  REQUIRE_FALSE(stack.undoLabel().has_value());
  REQUIRE_FALSE(stack.redoLabel().has_value());
  REQUIRE(markerAt(world, entity) == 10.0f);
}

TEST_CASE("undo restores the world as it was recorded", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");

  world.replace<Pose>(entity, marked(20.0f));

  REQUIRE(markerAt(world, entity) == 20.0f);
  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 10.0f);
}

TEST_CASE("redo puts the change back", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");

  world.replace<Pose>(entity, marked(20.0f));

  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 10.0f);

  REQUIRE(stack.redo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);
}

TEST_CASE("a stack walks back through several steps", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "First");
  world.replace<Pose>(entity, marked(20.0f));

  stack.record(world, "Second");
  world.replace<Pose>(entity, marked(30.0f));

  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);

  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 10.0f);

  REQUIRE_FALSE(stack.undo(world));

  REQUIRE(stack.redo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);

  REQUIRE(stack.redo(world));
  REQUIRE(markerAt(world, entity) == 30.0f);

  REQUIRE_FALSE(stack.redo(world));
}

TEST_CASE("recording drops the redo side", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");
  world.replace<Pose>(entity, marked(20.0f));

  REQUIRE(stack.undo(world));
  REQUIRE(stack.redoLabel().has_value());

  stack.record(world, "Elsewhere");
  world.replace<Pose>(entity, marked(30.0f));

  REQUIRE_FALSE(stack.redoLabel().has_value());
  REQUIRE_FALSE(stack.redo(world));
  REQUIRE(markerAt(world, entity) == 30.0f);
}

TEST_CASE("the oldest step falls off a full stack", "[undo]") {
  World world;
  UndoStack stack{2};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "First");
  world.replace<Pose>(entity, marked(20.0f));

  stack.record(world, "Second");
  world.replace<Pose>(entity, marked(30.0f));

  stack.record(world, "Third");
  world.replace<Pose>(entity, marked(40.0f));

  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 30.0f);

  REQUIRE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);

  REQUIRE_FALSE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);
}

TEST_CASE("each side names the step it holds", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");
  world.replace<Pose>(entity, marked(20.0f));

  REQUIRE(stack.undoLabel() == "Move");
  REQUIRE_FALSE(stack.redoLabel().has_value());

  REQUIRE(stack.undo(world));

  REQUIRE_FALSE(stack.undoLabel().has_value());
  REQUIRE(stack.redoLabel() == "Move");
}

TEST_CASE("clearing forgets both sides", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");
  world.replace<Pose>(entity, marked(20.0f));

  stack.clear();

  REQUIRE_FALSE(stack.undo(world));
  REQUIRE_FALSE(stack.redo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);
}

TEST_CASE("a depth of zero records nothing", "[undo]") {
  World world;
  UndoStack stack{0};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  stack.record(world, "Move");
  world.replace<Pose>(entity, marked(20.0f));

  REQUIRE_FALSE(stack.undo(world));
  REQUIRE(markerAt(world, entity) == 20.0f);
}

TEST_CASE("a destroyed entity comes back whole", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  REQUIRE(world.addPrimitive(
    entity,
    dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 7u)
  ));

  world.emplaceOrReplace<Material>(entity, Material{4u});
  world.emplaceOrReplace<MassScale>(entity, MassScale{2.5f});
  world.addStaticBody(entity);

  dent(world, entity);

  stack.record(world, "Delete");

  REQUIRE(world.destroy(entity));
  REQUIRE(dunya::objectmodel::liveEntities(world).empty());

  REQUIRE(stack.undo(world));

  REQUIRE(world.registry().all_of<SdfGrid>(entity));
  REQUIRE(markerAt(world, entity) == 10.0f);

  REQUIRE(world.primitiveCount(entity) == 1);
  REQUIRE(world.primitives(entity)[0].shapeConfig.y == 7u);

  REQUIRE(world.registry().get<Material>(entity).index == 4u);
  REQUIRE(world.registry().get<MassScale>(entity).factor == 2.5f);
  REQUIRE(world.registry().all_of<StaticBody>(entity));
  REQUIRE(world.registry().all_of<Deformable>(entity));
  REQUIRE(world.registry().all_of<Deformed>(entity));

  REQUIRE(world.hasSampledSdf(entity));
  REQUIRE(world.sampledSdf(entity)->distances[0] == -1.0f);
}

TEST_CASE("a recorded step outlives later carving", "[undo]") {
  World world;
  UndoStack stack{8};

  const Entity entity = world.createSdfGrid(marked(10.0f), blank());

  dent(world, entity);

  stack.record(world, "Carve");

  world.patchSampledSdf(entity, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[1] = -2.0f;
  });

  REQUIRE(world.sampledSdf(entity)->distances[1] == -2.0f);

  REQUIRE(stack.undo(world));

  REQUIRE(world.sampledSdf(entity)->distances[0] == -1.0f);
  REQUIRE(world.sampledSdf(entity)->distances[1] == 1.0f);
}
