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
#include <dunya/objectmodel/deformable/deformable.h>
#include <dunya/objectmodel/instantiate/instantiate.h>
#include <dunya/objectmodel/staticbody/staticbody.h>
#include <dunya/objectmodel/world/world.h>

#include <cstdint>

namespace {

using dunya::objectmodel::BakedVolume;
using dunya::objectmodel::Entity;
using dunya::objectmodel::Material;
using dunya::objectmodel::Mesh;
using dunya::objectmodel::Pose;
using dunya::objectmodel::SdfGrid;
using dunya::objectmodel::World;

// A grid is only usable once it has a resolution; the marker rides in the Pose
// beside it, so a test can say which entity it is looking at after
// instantiation has moved everything into a second registry.
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
  REQUIRE(runtime.registry().all_of<SdfGrid>(first));
  REQUIRE(runtime.registry().all_of<SdfGrid>(second));
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

  REQUIRE(runtime.registry().all_of<SdfGrid>(zero));
  REQUIRE(runtime.registry().all_of<SdfGrid>(five));
  REQUIRE_FALSE(runtime.registry().all_of<SdfGrid>(Entity{1}));
  REQUIRE(markerAt(runtime, five) == 15.0f);
}

TEST_CASE("primitives arrive in csg order", "[instantiate]") {
  // Primitive order is semantically meaningful to the fold, so a copy that
  // preserves the set but not the sequence produces a different shape.
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());

  REQUIRE(authored.addPrimitive(id, marker(1)));
  REQUIRE(authored.addPrimitive(id, marker(2)));
  REQUIRE(authored.addPrimitive(id, marker(3)));

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

TEST_CASE("mesh entities arrive in order", "[instantiate]") {
  // Indices into the assets the two worlds share, so these copy verbatim.
  World authored;

  authored.createMesh(Pose{}, Mesh{0}, Material{2});
  authored.createMesh(Pose{}, Mesh{1}, Material{4});

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  const std::span<const Entity> meshes = runtime.meshes();

  REQUIRE(meshes.size() == 2);
  REQUIRE(runtime.registry().get<Mesh>(meshes[0]).index == 0);
  REQUIRE(runtime.registry().get<Material>(meshes[0]).index == 2);
  REQUIRE(runtime.registry().get<Mesh>(meshes[1]).index == 1);
  REQUIRE(runtime.registry().get<Material>(meshes[1]).index == 4);
}

TEST_CASE("instantiating leaves the authored world alone", "[instantiate]") {
  World authored;

  const Entity id = authored.createField(marked(10.0f), blank());
  REQUIRE(authored.addPrimitive(id, marker(1)));
  authored.createMesh(Pose{}, Mesh{0}, Material{0});

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(authored.fields().size() == 1);
  REQUIRE(authored.primitiveCount(id) == 1);
  REQUIRE(authored.meshes().size() == 1);
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
  REQUIRE(authored.addPrimitive(id, marker(1)));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  // Simulate: the body moves and something carves a dent into it.
  runtime.replace<Pose>(
    id,
    Pose{glm::vec3(99.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)}
  );
  REQUIRE(runtime.addPrimitive(id, marker(2)));
  runtime.createMesh(Pose{}, Mesh{7}, Material{7});

  // Stop.
  REQUIRE(markerAt(authored, id) == 10.0f);
  REQUIRE(authored.primitiveCount(id) == 1);
  REQUIRE(materialAt(authored, id, 0) == 1);
  REQUIRE(authored.meshes().size() == 0);
}

TEST_CASE("mesh entities arrive at the ids they had", "[instantiate]") {
  // The order case above would pass with fresh ids. At M18 a mesh entity
  // carries a physics body, and a body keyed off an id that changed on the way
  // across is a silent bug, so identity is pinned separately here.
  World authored;

  const Entity two{2};
  const Entity seven{7};

  REQUIRE(authored.createMeshAt(two, Pose{}, Mesh{0}, Material{1}));
  REQUIRE(authored.createMeshAt(seven, Pose{}, Mesh{1}, Material{2}));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().all_of<Mesh>(two));
  REQUIRE(runtime.registry().all_of<Mesh>(seven));
  REQUIRE_FALSE(runtime.registry().all_of<Mesh>(Entity{3}));
  REQUIRE(runtime.registry().get<Material>(seven).index == 2);
}

TEST_CASE("a static body stays static across instantiation", "[instantiate]") {
  // The copy names its components one at a time, which is what keeps
  // BakedVolume out. The same list silently drops anything nobody adds to it,
  // and an untagged ground falls through the world instead of holding it up.
  World authored;

  const Entity ground = authored.createField(marked(10.0f), blank());
  const Entity faller = authored.createField(marked(11.0f), blank());

  authored.addStaticBody(ground);

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().all_of<dunya::objectmodel::StaticBody>(ground));
  REQUIRE_FALSE(
    runtime.registry().all_of<dunya::objectmodel::StaticBody>(faller)
  );
}

TEST_CASE(
  "a deformable stays deformable across instantiation",
  "[instantiate]"
) {
  static_assert(
    dunya::objectmodel::selfContained<dunya::objectmodel::Deformable>,
    "Deformable has to be SelfContained or emplaceOrReplace refuses it"
  );

  World authored;

  const Entity dented = authored.createField(marked(12.0f), blank());
  const Entity rigid = authored.createField(marked(13.0f), blank());

  authored.emplaceOrReplace<dunya::objectmodel::Deformable>(
    dented,
    dunya::objectmodel::Deformable{}
  );

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().all_of<dunya::objectmodel::Deformable>(dented));
  REQUIRE_FALSE(
    runtime.registry().all_of<dunya::objectmodel::Deformable>(rigid)
  );
}

TEST_CASE(
  "a dented lattice stays dented across instantiation",
  "[instantiate]"
) {
  // The copy carries the dents, so it must carry the fact that it has them.
  // Without this the runtime object looks like a pristine bake of its
  // primitives, and anything sharing on that basis hands it somebody else's
  // geometry. Ordering matters as much as presence: setSampledField clears
  // the mark, so a copy made before the field would be undone by it.
  static_assert(
    dunya::objectmodel::selfContained<dunya::objectmodel::Deformed>,
    "Deformed has to be SelfContained or emplaceOrReplace refuses it"
  );

  World authored;

  const Entity dented = authored.createField(marked(14.0f), blank());
  const Entity pristine = authored.createField(marked(15.0f), blank());

  for (const Entity entity : {dented, pristine}) {
    authored.emplaceOrReplace<dunya::objectmodel::Deformable>(
      entity,
      dunya::objectmodel::Deformable{}
    );

    dunya::field::SampledField field;
    field.resolution = glm::uvec3(2u);
    field.distances.assign(8u, 1.0f);
    field.materials.assign(8u, 0u);

    authored.setSampledField(entity, std::move(field));
  }

  authored.patchSampledField(dented, [](dunya::field::SampledField& lattice) {
    lattice.distances[0] = -1.0f;
  });

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.registry().all_of<dunya::objectmodel::Deformed>(dented));
  REQUIRE_FALSE(
    runtime.registry().all_of<dunya::objectmodel::Deformed>(pristine)
  );
}

TEST_CASE("the runtime shares the authored lattice", "[instantiate]") {
  // Play used to copy every lattice into the runtime world: 1.2 MB an object,
  // and 720 MB for a level of them. It hands over the handle now, and the
  // runtime's first dent takes a private copy through patchSampledField.
  World authored;

  const Entity entity = authored.createField(marked(16.0f), blank());

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  authored.setSampledField(entity, std::move(field));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.sampledField(entity) == authored.sampledField(entity));
  REQUIRE(authored.sampledFieldUsers(entity) == 2);
}
