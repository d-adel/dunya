#pragma once

#include <dunya/objectmodel/world/world.h>
#include <dunya/systems/input/input.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dunya::systems {

struct Context {
  dunya::objectmodel::World& world;
  const InputState& input;
  float deltaSeconds = 0.0f;
  uint32_t frameIndex = 0;
};

using Function = std::function<void(Context&)>;

struct Entry {
  int32_t order = 0;
  std::string name;
  Function function;
  bool enabled = true;
  double lastMilliseconds = 0.0;
};

class Schedule {
public:
  [[nodiscard]] bool add(int32_t order, std::string name, Function function);

  [[nodiscard]] bool remove(std::string_view name);

  [[nodiscard]] bool enable(std::string_view name, bool wanted);

  [[nodiscard]] bool enabled(std::string_view name) const noexcept;

  void clear() noexcept;

  void run(Context& context);

  [[nodiscard]] std::span<const Entry> systems() const noexcept;

  [[nodiscard]] size_t size() const noexcept;

  [[nodiscard]] double lastMilliseconds() const noexcept;

  [[nodiscard]] double lastMilliseconds(std::string_view name) const noexcept;

private:
  [[nodiscard]] Entry* find(std::string_view name) noexcept;
  [[nodiscard]] const Entry* find(std::string_view name) const noexcept;

  std::vector<Entry> m_systems;
  double m_lastMilliseconds = 0.0;
};

}
