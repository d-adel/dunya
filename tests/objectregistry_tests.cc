#include <catch2/catch_test_macros.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/objectregistry/objectregistry.h>

#include <cstdint>

namespace {

using dunya::core::ObjectId;
using dunya::objectmodel::FieldObject;
using dunya::objectmodel::ObjectRegistry;

// position.x carries a marker, which is how a test tells one object from
// another after the allocator has had a chance to confuse them.
FieldObject marked(float marker) {
  FieldObject object{};
  object.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);
  object.position.x = marker;

  return object;
}

float markerAt(const ObjectRegistry& registry, ObjectId id) {
  return registry.getFieldObject(id).position.x;
}

}  // namespace

TEST_CASE(
  "placing objects by id claims those ids from the allocator",
  "[objectregistry]"
) {
  // The editor/runtime instantiation places every object at the id it had in
  // the authored world, so the allocator has to learn about ids it did not
  // hand out. Undo and redo never exposed this: they restore ids that
  // removeFieldObject already pushed onto the free queue.
  ObjectRegistry registry;

  REQUIRE(registry.addFieldObjectAt(0, marked(10.0f)));
  REQUIRE(registry.addFieldObjectAt(1, marked(11.0f)));
  REQUIRE(registry.addFieldObjectAt(2, marked(12.0f)));

  const ObjectId fresh = registry.addFieldObject(marked(99.0f));

  REQUIRE(fresh != 0);
  REQUIRE(fresh != 1);
  REQUIRE(fresh != 2);
}

TEST_CASE(
  "adding an object never returns the id of a different object",
  "[objectregistry]"
) {
  // The sharp form of the same bug: not a collision, an alias. A caller that
  // receives someone else's id edits the wrong object for the rest of its
  // life, and nothing reports an error.
  ObjectRegistry registry;

  REQUIRE(registry.addFieldObjectAt(0, marked(10.0f)));

  const ObjectId fresh = registry.addFieldObject(marked(99.0f));

  REQUIRE(fresh != dunya::core::INVALID_OBJECT_ID);
  REQUIRE(markerAt(registry, fresh) == 99.0f);
  REQUIRE(markerAt(registry, 0) == 10.0f);
}

TEST_CASE(
  "an object placed by id survives a later allocation",
  "[objectregistry]"
) {
  // Stated from the placed object's side rather than the allocator's: whatever
  // addFieldObject does next, it must not overwrite what is already there.
  // Slot 0 on purpose, because that is the one the allocator will pick.
  ObjectRegistry registry;

  REQUIRE(registry.addFieldObjectAt(0, marked(44.0f)));

  registry.addPrimitive(0, dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 7));

  registry.addFieldObject(marked(99.0f));

  REQUIRE(registry.contains(0));
  REQUIRE(markerAt(registry, 0) == 44.0f);
  REQUIRE(registry.primitiveCount(0) == 1);
  REQUIRE(registry.getPrimitives(0)[0].shapeConfig.y == 7);
}

TEST_CASE(
  "placing an id twice is refused and leaves the first object alone",
  "[objectregistry]"
) {
  ObjectRegistry registry;

  REQUIRE(registry.addFieldObjectAt(3, marked(33.0f)));
  REQUIRE_FALSE(registry.addFieldObjectAt(3, marked(99.0f)));

  REQUIRE(markerAt(registry, 3) == 33.0f);
  REQUIRE(registry.fieldObjectCount() == 1);
}

TEST_CASE(
  "a removed id is recycled lowest first",
  "[objectregistry]"
) {
  // Pins the behaviour the fix must not break: the free queue is a
  // priority_queue with std::greater precisely so ids stay compact.
  ObjectRegistry registry;

  const ObjectId first = registry.addFieldObject(marked(1.0f));
  const ObjectId second = registry.addFieldObject(marked(2.0f));
  const ObjectId third = registry.addFieldObject(marked(3.0f));

  REQUIRE(registry.removeFieldObject(third));
  REQUIRE(registry.removeFieldObject(first));

  REQUIRE(registry.addFieldObject(marked(4.0f)) == first);
  REQUIRE(registry.addFieldObject(marked(5.0f)) == third);
  REQUIRE(registry.contains(second));
}
