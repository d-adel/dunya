#include "undostack.ih"

namespace dunya::undo {

using dunya::objectmodel::World;

UndoStack::UndoStack(size_t depth) : m_depth(depth) {}

UndoStack::Step UndoStack::capture(const World& world, std::string label) {
  Step step{std::make_unique<World>(), std::move(label)};

  dunya::objectmodel::instantiateWorld(world, *step.world);

  return step;
}

void UndoStack::restore(const World& source, World& destination) {
  destination.clear();

  dunya::objectmodel::instantiateWorld(source, destination);
}

void UndoStack::record(const World& world, std::string label) {
  if (m_depth == 0) {
    return;
  }

  m_undo.push_back(capture(world, std::move(label)));
  m_redo.clear();

  while (m_undo.size() > m_depth) {
    m_undo.pop_front();
  }
}

bool UndoStack::undo(World& world) {
  if (m_undo.empty()) {
    return false;
  }

  Step step = std::move(m_undo.back());
  m_undo.pop_back();

  m_redo.push_back(capture(world, step.label));

  restore(*step.world, world);

  return true;
}

bool UndoStack::redo(World& world) {
  if (m_redo.empty()) {
    return false;
  }

  Step step = std::move(m_redo.back());
  m_redo.pop_back();

  m_undo.push_back(capture(world, step.label));

  restore(*step.world, world);

  return true;
}

void UndoStack::clear() noexcept {
  m_undo.clear();
  m_redo.clear();
}

std::optional<std::string_view> UndoStack::undoLabel() const noexcept {
  if (m_undo.empty()) {
    return std::nullopt;
  }

  return std::string_view{m_undo.back().label};
}

std::optional<std::string_view> UndoStack::redoLabel() const noexcept {
  if (m_redo.empty()) {
    return std::nullopt;
  }

  return std::string_view{m_redo.back().label};
}

}
