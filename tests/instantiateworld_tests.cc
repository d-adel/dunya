/* The specification for the editor/runtime split, written before the code it
 * describes. Not on the field_tests target yet: instantiateWorld does not
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

using dunya::objectmodel::DrawItem;
using dunya::objectmodel::Entity;
using dunya::objectmodel::FieldGrid;
using dunya::objectmodel::World;

// position.x carries a marker, so a test can say which object it is looking at
// after instantiation has moved everything into a second registry.
FieldGrid marked(float marker) {
  FieldGrid object{};
  object.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);
  object.position.x = marker;

  return object;
}

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// from another once they are spans in a different pool.
dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

float markerAt(const World& world, Entity entity) {
  return world.registry().get<FieldGrid>(entity).position.x;
}

uint32_t materialAt(const World& world, Entity entity, uint32_t index) {
  return world.primitives(entity)[index].shapeConfig.y;
}

}  // namespace

TEST_CASE("every object arrives at the id it had", "[instantiate]") {
  // The whole design keys physics bodies, sampled fields and GPU volumes off
  // one number, so a translation table is exactly what this must avoid.
  World authored;

  const Entity first = authored.addFieldObject(marked(10.0f));
  const Entity second = authored.addFieldObject(marked(11.0f));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.fieldObjects().size() == 2);
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

  REQUIRE(authored.addFieldObjectAt(zero, marked(10.0f)));
  REQUIRE(authored.addFieldObjectAt(five, marked(15.0f)));

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

  const Entity id = authored.addFieldObject(marked(10.0f));

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

TEST_CASE("volumeIndex does not cross the boundary", "[instantiate]") {
  // Bug B2. The frame loop bakes only when volumeIndex is UINT32_MAX, so an
  // instantiated object that keeps the editor's index renders the editor's
  // volume and dents it. Copying FieldGrid wholesale gets this wrong.
  World authored;

  const Entity id = authored.addFieldObject(marked(10.0f));
  authored.setVolumeIndex(id, 3);

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().get<FieldGrid>(id).volumeIndex == UINT32_MAX);
  REQUIRE(authored.registry().get<FieldGrid>(id).volumeIndex == 3);
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

  const Entity id = authored.addFieldObject(marked(10.0f));
  authored.addPrimitive(id, marker(1));
  authored.addDrawItem(DrawItem{0, 0, glm::mat4(1.0f)});

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(authored.fieldObjects().size() == 1);
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

  const Entity id = authored.addFieldObject(marked(10.0f));
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
