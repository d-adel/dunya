#pragma once

#include <dunya/objectmodel/world/world.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dunya::undo {

class UndoStack {
public:
  explicit UndoStack(size_t depth);

  UndoStack(const UndoStack&) = delete;
  UndoStack& operator=(const UndoStack&) = delete;
  UndoStack(UndoStack&&) = delete;
  UndoStack& operator=(UndoStack&&) = delete;

  ~UndoStack() = default;

  void record(const dunya::objectmodel::World& world, std::string label);

  [[nodiscard]]
  bool undo(dunya::objectmodel::World& world);

  [[nodiscard]]
  bool redo(dunya::objectmodel::World& world);

  void clear() noexcept;

  [[nodiscard]]
  std::optional<std::string_view> undoLabel() const noexcept;

  [[nodiscard]]
  std::optional<std::string_view> redoLabel() const noexcept;

private:
  struct Step {
    std::unique_ptr<dunya::objectmodel::World> world;
    std::string label;
  };

  [[nodiscard]]
  static Step capture(
    const dunya::objectmodel::World& world,
    std::string label
  );

  static void restore(
    const dunya::objectmodel::World& source,
    dunya::objectmodel::World& destination
  );

  std::deque<Step> m_undo;

  std::vector<Step> m_redo;

  size_t m_depth;
};

}
