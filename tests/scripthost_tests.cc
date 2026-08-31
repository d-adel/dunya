#include <catch2/catch_test_macros.hpp>

#include <dunya/script/host/host.h>

#include <filesystem>

using dunya::script::Host;

namespace {

std::filesystem::path managedDirectory() {
  return std::filesystem::path(DUNYA_MANAGED_DIR);
}

}

TEST_CASE(
  "the runtime loads and a managed entry point resolves",
  "[scripthost]"
) {
  Host host;

  REQUIRE(host.start(managedDirectory() / "Dunya.Engine.runtimeconfig.json"));
  REQUIRE(host.running());

  void* found = host.entryPoint(
    managedDirectory() / "Dunya.Engine.dll",
    "Dunya.Engine.Boot, Dunya.Engine",
    "Initialize"
  );

  REQUIRE(found != nullptr);
}

TEST_CASE("a missing configuration is reported, not crashed", "[scripthost]") {
  Host host;

  REQUIRE_FALSE(host.start(managedDirectory() / "absent.runtimeconfig.json"));
  REQUIRE_FALSE(host.running());
  REQUIRE_FALSE(host.lastError().empty());
}

TEST_CASE("an absent entry point is reported, not crashed", "[scripthost]") {
  Host host;

  REQUIRE(host.start(managedDirectory() / "Dunya.Engine.runtimeconfig.json"));

  REQUIRE(
    host.entryPoint(
      managedDirectory() / "Dunya.Engine.dll",
      "Dunya.Engine.Boot, Dunya.Engine",
      "NoSuchMethod"
    )
    == nullptr
  );

  REQUIRE_FALSE(host.lastError().empty());
}
