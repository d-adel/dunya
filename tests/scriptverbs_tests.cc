#include <catch2/catch_test_macros.hpp>

#include "fieldprimitives.h"

#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/script/api/api.h>

namespace {

struct Recorder {
  uint32_t entity = UINT32_MAX;
  float mass = 0.0f;
  float velocity[3]{};
  uint32_t destroyed = UINT32_MAX;
};

int32_t recordBody(void* host, void*, uint32_t entity, float mass) {
  auto* seen = static_cast<Recorder*>(host);

  seen->entity = entity;
  seen->mass = mass;

  return 1;
}

int32_t recordVelocity(void* host, void*, uint32_t entity, const float* value) {
  auto* seen = static_cast<Recorder*>(host);

  seen->entity = entity;
  seen->velocity[0] = value[0];
  seen->velocity[1] = value[1];
  seen->velocity[2] = value[2];

  return 1;
}

int32_t recordDestroy(void* host, void*, uint32_t entity) {
  static_cast<Recorder*>(host)->destroyed = entity;

  return 1;
}

constexpr dunya::script::PhysicsVerbs RECORDING{
  &recordBody,
  &recordVelocity,
  &recordDestroy
};

}

TEST_CASE("a script builds an object out of general verbs", "[scriptverbs]") {
  dunya::objectmodel::World world;

  const dunya::script::Api& api = dunya::script::api();

  const float pose[7]{1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  const uint32_t resolution[3]{33u, 33u, 33u};

  const uint32_t made = api.createSdfGrid(&world, pose, resolution, 0.0f);

  REQUIRE(made != UINT32_MAX);

  const auto entity =
    static_cast<dunya::objectmodel::Entity>(entt::entity{made});

  REQUIRE(world.registry().all_of<dunya::objectmodel::SdfGrid>(entity));
  REQUIRE(
    world.registry().get<dunya::objectmodel::Pose>(entity).position
    == glm::vec3(1.0f, 2.0f, 3.0f)
  );
  REQUIRE(
    world.registry().get<dunya::objectmodel::SdfGrid>(entity).resolution
    == glm::uvec3(33u)
  );

  dunya::script::SdfEditDescriptor shape{};
  shape.kind = 0u;
  shape.material = 1u;
  shape.size[0] = 0.35f;
  shape.position[0] = 1.0f;
  shape.position[1] = 2.0f;
  shape.position[2] = 3.0f;
  shape.rotation[3] = 1.0f;

  REQUIRE(api.addPrimitive(&world, made, &shape) == 1);
  REQUIRE(world.primitiveCount(entity) == 1u);

  const glm::vec3 centre =
    glm::vec3(glm::inverse(world.primitives(entity)[0].inverseModel)[3]);

  REQUIRE(glm::length(centre) < 1.0e-4f);
}

TEST_CASE(
  "the physics verbs reach whichever host installed them",
  "[scriptverbs]"
) {
  dunya::objectmodel::World world;

  const dunya::script::Api& api = dunya::script::api();

  Recorder seen;

  {
    const dunya::script::PhysicsScope scope(&RECORDING, &seen);

    const float velocity[3]{0.0f, 0.0f, -22.0f};

    REQUIRE(api.setRigidBody(&world, 7u, 150.0f) == 1);
    REQUIRE(api.setVelocity(&world, 7u, velocity) == 1);
    REQUIRE(api.destroy(&world, 7u) == 1);
  }

  REQUIRE(seen.entity == 7u);
  REQUIRE(seen.mass == 150.0f);
  REQUIRE(seen.velocity[2] == -22.0f);
  REQUIRE(seen.destroyed == 7u);

  REQUIRE(api.setRigidBody(&world, 7u, 1.0f) == 0);
  REQUIRE(api.setVelocity(&world, 7u, nullptr) == 0);
}

TEST_CASE("a shared lattice is one lattice", "[scriptverbs]") {
  dunya::objectmodel::World world;

  const dunya::script::Api& api = dunya::script::api();

  const float pose[7]{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  const uint32_t resolution[3]{17u, 17u, 17u};

  const uint32_t donor = api.createSdfGrid(&world, pose, resolution, 0.0f);
  const uint32_t taker = api.createSdfGrid(&world, pose, resolution, 0.0f);

  REQUIRE(api.shareSdf(&world, donor, taker) == 0);

  const auto donorEntity =
    static_cast<dunya::objectmodel::Entity>(entt::entity{donor});
  const auto takerEntity =
    static_cast<dunya::objectmodel::Entity>(entt::entity{taker});

  dunya::field::SampledSdf lattice{};
  lattice.resolution = glm::uvec3(4u);
  lattice.distances.assign(64u, 1.0f);
  lattice.materials.assign(64u, 0u);

  world.setSampledSdf(donorEntity, std::move(lattice));

  REQUIRE(api.shareSdf(&world, donor, taker) == 1);
  REQUIRE(world.sampledSdf(donorEntity) == world.sampledSdf(takerEntity));
}
