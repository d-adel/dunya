#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/component/deformed/deformed.h>
#include <dunya/objectmodel/component/renderpose/renderpose.h>
#include <dunya/objectmodel/trait/authoredcomponents/authoredcomponents.h>
#include <dunya/objectmodel/trait/transientcomponents/transientcomponents.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldcontents/worldcontents.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

using dunya::objectmodel::AuthoredComponents;
using dunya::objectmodel::Deformed;
using dunya::objectmodel::Entity;
using dunya::objectmodel::RenderPose;
using dunya::objectmodel::TransientComponents;
using dunya::objectmodel::World;

std::vector<std::string> sorted(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());

  names.erase(std::unique(names.begin(), names.end()), names.end());

  return names;
}

std::vector<std::string> classifiedNames() {
  std::vector<std::string> names = dunya::objectmodel::authoredComponentNames();

  const std::vector<std::string> transient =
    dunya::objectmodel::transientComponentNames();

  names.insert(names.end(), transient.begin(), transient.end());

  return sorted(names);
}

}

TEST_CASE("every storage the world opens is classified", "[coverage]") {
  World world;

  const Entity entity = world.createAuthored();

  AuthoredComponents::each([&]<typename T>() {
    world.emplaceAuthored<T>(entity, T{});
  });

  world.setBakedVolume(entity, 0u);
  world.setRigidBody(entity, 0u);
  world.emplaceOrReplace<Deformed>(entity, Deformed{});
  world.emplaceOrReplace<RenderPose>(entity, RenderPose{});

  const std::vector<std::string> registered =
    sorted(dunya::objectmodel::registeredComponentNames(world));

  const std::vector<std::string> classified = classifiedNames();

  for (const std::string& name : registered) {
    INFO("component is neither authored nor transient: " << name);
    REQUIRE(
      std::find(classified.begin(), classified.end(), name) != classified.end()
    );
  }

  REQUIRE(
    registered.size() >= dunya::objectmodel::authoredComponentNames().size()
  );
}

TEST_CASE("no component is classified twice", "[coverage]") {
  const std::vector<std::string> authored =
    sorted(dunya::objectmodel::authoredComponentNames());

  const std::vector<std::string> transient =
    sorted(dunya::objectmodel::transientComponentNames());

  for (const std::string& name : transient) {
    INFO("classified both authored and transient: " << name);
    REQUIRE(
      std::find(authored.begin(), authored.end(), name) == authored.end()
    );
  }
}

TEST_CASE("the classification lists have no duplicates", "[coverage]") {
  const std::vector<std::string> authored =
    dunya::objectmodel::authoredComponentNames();

  const std::vector<std::string> transient =
    dunya::objectmodel::transientComponentNames();

  REQUIRE(sorted(authored).size() == authored.size());
  REQUIRE(sorted(transient).size() == transient.size());
}
