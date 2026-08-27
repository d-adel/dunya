#pragma once

#include <dunya/editor/command/command.h>
#include <dunya/objectmodel/world/world.h>

#include <vector>

namespace dunya::editor {

class CommandHistory {
public:
  template<typename T>
  bool execute(T command, dunya::objectmodel::World& world) {
    Command wrapped{std::move(command)};

    if (!apply(wrapped, world)) {
      return false;
    }

    m_undo.push_back(std::move(wrapped));
    m_redo.clear();

    return true;
  }

  bool execute(
    AddFieldObjectCommand& command,
    dunya::objectmodel::World& world
  ) {
    Command wrapped{command};

    if (!apply(wrapped, world)) {
      return false;
    }

    command = std::get<AddFieldObjectCommand>(wrapped);

    m_undo.push_back(std::move(wrapped));
    m_redo.clear();

    return true;
  }

  void undo(dunya::objectmodel::World& world);
  void redo(dunya::objectmodel::World& world);
  void clear();

  bool canUndo() const {
    return !m_undo.empty();
  }

  bool canRedo() const {
    return !m_redo.empty();
  }

private:
  static bool apply(Command& command, dunya::objectmodel::World& world);

  static bool revert(const Command& command, dunya::objectmodel::World& world);

  std::vector<Command> m_undo;
  std::vector<Command> m_redo;
};

}  // namespace dunya::editor
