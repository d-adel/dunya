#include <catch2/catch_test_macros.hpp>

#include "fieldprimitives.h"

#include <dunya/field/sampledsdf/sampledsdf.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/instantiate/instantiate.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/script/api/api.h>
#include <dunya/script/runner/runner.h>
#include <dunya/systems/schedule/schedule.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <climits>
#include <filesystem>

using dunya::objectmodel::ComponentType;
using dunya::objectmodel::Entity;
using dunya::objectmodel::INVALID_COMPONENT_TYPE;
using dunya::objectmodel::World;
using dunya::script::Runner;
using dunya::systems::Context;
using dunya::systems::Schedule;

namespace {

std::filesystem::path managedDirectory() {
  return std::filesystem::path(DUNYA_MANAGED_DIR);
}

std::filesystem::path scriptsDirectory() {
  return std::filesystem::path(DUNYA_SCRIPTS_DIR);
}

Entity deformableSphere(World& world, float radius) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(33u);

  const Entity entity = world.createSdfGrid({}, grid);

  REQUIRE(world.addPrimitive(entity, fixture::sphere(glm::vec3(0.0f), radius)));

  const dunya::field::Aabb box =
    dunya::objectmodel::gridBox(world.primitives(entity));

  world.setSampledSdf(
    entity,
    dunya::field::bake(
      world.primitives(entity),
      box.minimum,
      box.maximum,
      grid.resolution
    )
  );

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  return entity;
}

uint32_t solidCells(const World& world, Entity entity) {
  const dunya::field::SampledSdf* field = world.sampledSdf(entity);

  uint32_t solid = 0u;

  for (const float distance : field->distances) {
    if (distance < 0.0f) {
      ++solid;
    }
  }

  return solid;
}

std::array<std::byte, 28> carveRequest(
  uint32_t tool,
  const std::array<float, 3>& position,
  const std::array<float, 3>& size
) {
  std::array<std::byte, 28> value{};

  std::memcpy(value.data(), &tool, 4u);
  std::memcpy(value.data() + 4, position.data(), 12u);
  std::memcpy(value.data() + 16, size.data(), 12u);

  return value;
}

uint32_t idOf(Entity entity) {
  return static_cast<uint32_t>(entt::to_integral(entity));
}

}

TEST_CASE(
  "a project's scripts boot and register their systems",
  "[scriptbridge]"
) {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, scriptsDirectory()));
  REQUIRE(runner.running());

  REQUIRE(schedule.size() >= 1u);

  bool sawCarve = false;
  int32_t previous = INT32_MIN;

  for (const dunya::systems::Entry& entry : schedule.systems()) {
    REQUIRE(entry.order >= previous);

    previous = entry.order;

    if (entry.name == "carve") {
      sawCarve = true;

      REQUIRE(entry.order == 100);
    }
  }

  REQUIRE(sawCarve);
}

TEST_CASE(
  "the engine derives a component's layout from the script's struct",
  "[scriptbridge]"
) {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, scriptsDirectory()));

  const ComponentType type = world.dynamic().find("CarveRequest");

  REQUIRE(type != INVALID_COMPONENT_TYPE);

  const dunya::objectmodel::ComponentSpec* spec = world.dynamic().spec(type);

  REQUIRE(spec->size == 28u);
  REQUIRE(spec->fields.size() == 3u);

  REQUIRE(spec->fields[0].name == "tool");
  REQUIRE(spec->fields[0].offset == 0u);
  REQUIRE(spec->fields[0].kind == dunya::objectmodel::FieldKind::UInt);

  REQUIRE(spec->fields[1].name == "position");
  REQUIRE(spec->fields[1].offset == 4u);
  REQUIRE(spec->fields[1].kind == dunya::objectmodel::FieldKind::Vec3);

  REQUIRE(spec->fields[2].name == "size");
  REQUIRE(spec->fields[2].offset == 16u);
}

TEST_CASE("a script system carves the engine's field", "[scriptbridge]") {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, scriptsDirectory()));

  const Entity entity = deformableSphere(world, 1.0f);

  const uint32_t before = solidCells(world, entity);

  REQUIRE(before > 0u);

  const ComponentType request = world.dynamic().find("CarveRequest");

  REQUIRE(world.dynamic().emplace(
    request,
    entity,
    carveRequest(0u, {0.9f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f})
  ));

  Context context{world, 0.016f, 1u};
  schedule.run(context);

  REQUIRE(solidCells(world, entity) < before);
  REQUIRE_FALSE(world.dynamic().contains(request, entity));
}

TEST_CASE(
  "a script system leaves entities it did not match alone",
  "[scriptbridge]"
) {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, scriptsDirectory()));

  const Entity carved = deformableSphere(world, 1.0f);
  const Entity spared = deformableSphere(world, 1.0f);

  const uint32_t before = solidCells(world, spared);

  const ComponentType request = world.dynamic().find("CarveRequest");

  REQUIRE(world.dynamic().emplace(
    request,
    carved,
    carveRequest(0u, {0.9f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f})
  ));

  Context context{world, 0.016f, 1u};
  schedule.run(context);

  REQUIRE(solidCells(world, spared) == before);
  REQUIRE(world.dynamic().count(request) == 0u);
}

TEST_CASE(
  "every tool shape the script names reaches the field",
  "[scriptbridge]"
) {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, scriptsDirectory()));

  const ComponentType request = world.dynamic().find("CarveRequest");

  for (const uint32_t tool : {0u, 1u, 3u}) {
    const Entity entity = deformableSphere(world, 1.0f);

    const uint32_t before = solidCells(world, entity);

    REQUIRE(world.dynamic().emplace(
      request,
      entity,
      carveRequest(tool, {0.6f, 0.0f, 0.0f}, {0.4f, 0.4f, 0.4f})
    ));

    Context context{world, 0.016f, 1u};
    schedule.run(context);

    REQUIRE(solidCells(world, entity) < before);
  }
}

TEST_CASE(
  "a runner pointed at nothing reports rather than crashing",
  "[scriptbridge]"
) {
  Runner runner;

  REQUIRE_FALSE(runner.load(managedDirectory() / "absent"));
  REQUIRE_FALSE(runner.running());
  REQUIRE_FALSE(runner.lastError().empty());
}

TEST_CASE("a project with no scripts runs with no systems", "[scriptbridge]") {
  Runner runner;
  Schedule schedule;
  World world;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, world, managedDirectory() / "absent"));
  REQUIRE(runner.running());

  REQUIRE(schedule.size() == 0u);
  REQUIRE(world.dynamic().types() == 0u);
}

TEST_CASE(
  "declared component types survive instantiation with their indices",
  "[scriptbridge]"
) {
  Runner runner;
  Schedule schedule;
  World authored;

  REQUIRE(runner.load(managedDirectory()));
  REQUIRE(runner.initialize(schedule, authored, scriptsDirectory()));

  const ComponentType type = authored.dynamic().find("CarveRequest");

  const Entity entity = authored.createSdfGrid({}, {});

  REQUIRE(authored.dynamic().emplace(
    type,
    entity,
    carveRequest(3u, {1.0f, 2.0f, 3.0f}, {0.1f, 0.2f, 0.3f})
  ));

  World runtime;
  dunya::objectmodel::instantiateWorld(authored, runtime);

  REQUIRE(runtime.dynamic().find("CarveRequest") == type);
  REQUIRE(runtime.dynamic().contains(type, entity));

  uint32_t tool = 0u;
  std::memcpy(&tool, runtime.dynamic().get(type, entity), 4u);

  REQUIRE(tool == 3u);
}

TEST_CASE(
  "the material query reports what a cut would reach",
  "[scriptbridge]"
) {
  World world;

  const Entity entity = deformableSphere(world, 1.0f);

  dunya::script::SdfEditDescriptor inside{};
  inside.kind = 0u;
  inside.size[0] = 0.3f;
  inside.rotation[3] = 1.0f;

  std::array<uint32_t, 8> found{};

  const uint32_t count = dunya::script::api().materialsUnderSdf(
    &world,
    idOf(entity),
    &inside,
    found.data(),
    static_cast<uint32_t>(found.size())
  );

  REQUIRE(count >= 1u);
  REQUIRE(std::ranges::find(found, 1u) != found.end());

  dunya::script::SdfEditDescriptor outside{};
  outside.kind = 0u;
  outside.size[0] = 0.1f;
  outside.position[0] = 5.0f;
  outside.rotation[3] = 1.0f;

  REQUIRE(
    dunya::script::api().materialsUnderSdf(
      &world,
      idOf(entity),
      &outside,
      found.data(),
      static_cast<uint32_t>(found.size())
    )
    == 0u
  );
}

namespace {

void noopDeform(void*, uint32_t, const dunya::script::SdfDeformSummary*) {}

}

TEST_CASE(
  "a deform scope clears the global slot it installed",
  "[deformscope]"
) {
  int host = 0;

  REQUIRE(dunya::script::sdfDeformNotify() == nullptr);

  {
    const dunya::script::SdfDeformScope scope(&noopDeform, &host);

    REQUIRE(dunya::script::sdfDeformNotify() == &noopDeform);
    REQUIRE(dunya::script::sdfDeformHost() == &host);
  }

  REQUIRE(dunya::script::sdfDeformNotify() == nullptr);
  REQUIRE(dunya::script::sdfDeformHost() == nullptr);
}

TEST_CASE("a deform scope restores the one it displaced", "[deformscope]") {
  int outer = 0;
  int inner = 0;

  {
    const dunya::script::SdfDeformScope first(&noopDeform, &outer);

    {
      const dunya::script::SdfDeformScope second(&noopDeform, &inner);

      REQUIRE(dunya::script::sdfDeformHost() == &inner);
    }

    REQUIRE(dunya::script::sdfDeformHost() == &outer);
  }

  REQUIRE(dunya::script::sdfDeformNotify() == nullptr);
}

TEST_CASE(
  "assigning a deform scope releases the old registration",
  "[deformscope]"
) {
  int host = 0;

  dunya::script::SdfDeformScope scope(&noopDeform, &host);

  REQUIRE(dunya::script::sdfDeformHost() == &host);

  scope = {};

  REQUIRE(dunya::script::sdfDeformNotify() == nullptr);
  REQUIRE(dunya::script::sdfDeformHost() == nullptr);
}
