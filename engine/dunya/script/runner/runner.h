#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/script/host/host.h>
#include <dunya/systems/schedule/schedule.h>

#include <filesystem>
#include <string>

namespace dunya::script {

class Runner {
public:
  Runner() = default;
  ~Runner() = default;

  Runner(const Runner&) = delete;
  Runner& operator=(const Runner&) = delete;
  Runner(Runner&&) = delete;
  Runner& operator=(Runner&&) = delete;

  [[nodiscard]] bool load(const std::filesystem::path& managed);

  [[nodiscard]] bool initialize(
    dunya::systems::Schedule& schedule,
    dunya::objectmodel::World& world,
    const std::filesystem::path& scripts
  );

  [[nodiscard]] bool running() const noexcept;

  [[nodiscard]] const std::string& lastError() const noexcept;

private:
  Host m_host;

  std::filesystem::path m_managed;

  bool m_initialized = false;
};

}
