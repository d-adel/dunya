/* The specification for the editor/runtime split, written before the code it
 * describes. Not on the unit_tests target yet: instantiateWorld does not
 * exist, so this file cannot compile. Add it to tests/CMakeLists.txt the
 * moment it does. The reads and writes below are on the World surface
 * settled at M31 increment C, so only the function is missing.
 *
 * Assumed shape, and adjust these tests rather than the design if it differs:
 *
 *   namespace dunya::objectmodel {
 *     void instantiateWorld(const World& source, World& destination);
 *   }
 */

#include <catch2/catch_test_macros.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/world/world.h>

#include <cstdint>

namespace {

using dunya::objectmodel::BakedVolume;
using dunya::objectmodel::DrawItem;
using dunya::objectmodel::Entity;
using dunya::objectmodel::FieldGrid;
using dunya::objectmodel::Pose;
using dunya::objectmodel::World;

// A grid is only usable once it has a resolution; the marker rides in the Pose
// beside it, so a test can say which entity it is looking at after
// instantiation has moved everything into a second registry.
FieldGrid blank() {
  FieldGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  return grid;
}

Pose marked(float marker) {
  Pose pose{};
  pose.position.x = marker;

  return pose;
}

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// from another once they are spans in a different pool.
dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

float markerAt(const World& world, Entity entity) {
  return world.registry().get<Pose>(entity).position.x;
}

uint32_t materialAt(const World& world, Entity entity, uint32_t index) {
  return world.primitives(entity)[index].shapeConfig.y;
}

}  // namespace

TEST_CASE("every object arrives at the id it had", "[instantiate]") {
  // The whole design keys physics bodies, sampled fields and GPU volumes off
  // one number, so a translation table is exactly what this must avoid.
  World authored;

  const Entity first = authored.createField(marked(10.0f), blank());
  const Entity second = authored.createField(marked(11.0f), blank());

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.fields().size() == 2);
  REQUIRE(runtime.registry().all_of<FieldGrid>(first));
  REQUIRE(runtime.registry().all_of<FieldGrid>(second));
  REQUIRE(markerAt(runtime, first) == 10.0f);
  REQUIRE(markerAt(runtime, second) == 11.0f);
}

TEST_CASE("ids with holes in them are preserved", "[instantiate]") {
  // An authored world that has had objects deleted is not densely numbered,
  // and renumbering on the way across would break the keying above.
  World authored;

  const Entity zero{0};
  const Entity five{5};

  REQUIRE(authored.createFieldAt(zero, marked(10.0f), blank()));
  REQUIRE(authored.createFieldAt(five, marked(15.0f), blank()));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().all_of<FieldGrid>(zero));
  REQUIRE(runtime.registry().all_of<FieldGrid>(five));
  REQUIRE_FALSE(runtime.registry().all_of<FieldGrid>(Entity{1}));
  REQUIRE(markerAt(runtime, five) == 15.0f);
}

TEST_CASE("primitives arrive in csg order", "[instantiate]") {
  // Primitive order is semantically meaningful to the fold, so a copy that
  // preserves the set but not the sequence produces a different shape.
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());

  authored.addPrimitive(id, marker(1));
  authored.addPrimitive(id, marker(2));
  authored.addPrimitive(id, marker(3));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.primitiveCount(id) == 3);
  REQUIRE(materialAt(runtime, id, 0) == 1);
  REQUIRE(materialAt(runtime, id, 1) == 2);
  REQUIRE(materialAt(runtime, id, 2) == 3);
}

TEST_CASE("a baked volume does not cross the boundary", "[instantiate]") {
  // Bug B2. The frame loop allocates only for an entity with no BakedVolume,
  // so an instantiated entity that keeps the editor's component renders the
  // editor's volume and dents it. Naming the components to copy is what stops
  // this; a wholesale registry copy would take it.
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());
  authored.setBakedVolume(id, 3);

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  // Absence is the state, so the runtime entity must not carry the component
  // at all - not carry it holding a sentinel.
  REQUIRE_FALSE(runtime.registry().all_of<BakedVolume>(id));
  REQUIRE(authored.registry().get<BakedVolume>(id).index == 3);
}

TEST_CASE("draw items arrive in order", "[instantiate]") {
  // Indices into the assets the two worlds share, so these copy verbatim.
  World authored;

  authored.addDrawItem(DrawItem{0, 2, glm::mat4(1.0f)});
  authored.addDrawItem(DrawItem{1, 4, glm::mat4(2.0f)});

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.drawItems().size() == 2);
  REQUIRE(runtime.drawItems()[0].meshIndex == 0);
  REQUIRE(runtime.drawItems()[0].materialIndex == 2);
  REQUIRE(runtime.drawItems()[1].meshIndex == 1);
  REQUIRE(runtime.drawItems()[1].materialIndex == 4);
}

TEST_CASE("instantiating leaves the authored world alone", "[instantiate]") {
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());
  authored.addPrimitive(id, marker(1));
  authored.addDrawItem(DrawItem{0, 0, glm::mat4(1.0f)});

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(authored.fields().size() == 1);
  REQUIRE(authored.primitiveCount(id) == 1);
  REQUIRE(authored.drawItems().size() == 1);
  REQUIRE(markerAt(authored, id) == 10.0f);
}

TEST_CASE(
  "editing the runtime world does not reach the authored one",
  "[instantiate]"
) {
  // The acceptance criterion for the split, stated as Play, simulate, Stop.
  // Everything a runtime does to its world has to die with that world.
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());
  authored.addPrimitive(id, marker(1));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  // Simulate: the body moves and something carves a dent into it.
  runtime.setPose(
    id,
    glm::vec3(99.0f, 0.0f, 0.0f),
    glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
  );
  runtime.addPrimitive(id, marker(2));
  runtime.addDrawItem(DrawItem{7, 7, glm::mat4(1.0f)});

  // Stop.
  REQUIRE(markerAt(authored, id) == 10.0f);
  REQUIRE(authored.primitiveCount(id) == 1);
  REQUIRE(materialAt(authored, id, 0) == 1);
  REQUIRE(authored.drawItems().size() == 0);
}
