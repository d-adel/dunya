#pragma once

#include "command/command.h"

#include <vector>

class CommandHistory {
public:
  template<typename T>
  bool execute(T command, ObjectRegistry& registry) {
    Command wrapped{std::move(command)};

    if (!apply(wrapped, registry)) {
      return false;
    }

    m_undo.push_back(std::move(wrapped));
    m_redo.clear();

    return true;
  }

  bool execute(AddFieldObjectCommand& command, ObjectRegistry& registry) {
    Command wrapped{command};

    if (!apply(wrapped, registry)) {
      return false;
    }

    command = std::get<AddFieldObjectCommand>(wrapped);

    m_undo.push_back(std::move(wrapped));
    m_redo.clear();

    return true;
  }

  void undo(ObjectRegistry& registry);
  void redo(ObjectRegistry& registry);
  void clear();

  bool canUndo() const {
    return !m_undo.empty();
  }

  bool canRedo() const {
    return !m_redo.empty();
  }

private:
  static bool apply(Command& command, ObjectRegistry& registry);

  static bool revert(const Command& command, ObjectRegistry& registry);

  std::vector<Command> m_undo;
  std::vector<Command> m_redo;
};
