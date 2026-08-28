#include "commandhistory.ih"

#include <type_traits>
#include <utility>

namespace dunya::editor {

namespace {

// World deliberately has no contains. The liveness read goes through the const
// registry, the same predicate FieldEditor::addPrimitive already uses.
bool isField(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity
) {
  return world.registry().valid(entity)
         && world.registry().all_of<dunya::objectmodel::SdfGrid>(entity);
}

}  // namespace

void CommandHistory::undo(dunya::objectmodel::World& world) {
  if (m_undo.empty()) {
    return;
  }

  Command command = std::move(m_undo.back());
  m_undo.pop_back();

  if (!revert(command, world)) {
    m_undo.push_back(std::move(command));
    return;
  }

  m_redo.push_back(std::move(command));
}

void CommandHistory::redo(dunya::objectmodel::World& world) {
  if (m_redo.empty()) {
    return;
  }

  Command command = std::move(m_redo.back());
  m_redo.pop_back();

  if (!apply(command, world)) {
    m_redo.push_back(std::move(command));
    return;
  }

  m_undo.push_back(std::move(command));
}

void CommandHistory::clear() {
  m_undo.clear();
  m_redo.clear();
}

bool CommandHistory::apply(Command& command, dunya::objectmodel::World& world) {
  return std::visit(
    [&](auto& cmd) -> bool {
      using T = std::decay_t<decltype(cmd)>;

      if constexpr (std::is_same_v<T, CreateFieldCommand>) {
        if (cmd.entity == dunya::objectmodel::INVALID_ENTITY) {
          cmd.entity = world.createField(cmd.pose, cmd.grid);
          return cmd.entity != dunya::objectmodel::INVALID_ENTITY;
        }

        return world.createFieldAt(cmd.entity, cmd.pose, cmd.grid);
      }

      else if constexpr (std::is_same_v<T, DestroyFieldCommand>) {
        if (!cmd.grid.has_value()) {
          if (!isField(world, cmd.entity)) {
            return false;
          }

          cmd.pose = world.registry().get<dunya::objectmodel::Pose>(cmd.entity);

          cmd.grid =
            world.registry().get<dunya::objectmodel::SdfGrid>(cmd.entity);

          const auto primitives = world.primitives(cmd.entity);

          cmd.primitives.assign(primitives.begin(), primitives.end());
        }

        return world.destroyField(cmd.entity);
      }

      else if constexpr (std::is_same_v<T, TransformFieldCommand>) {
        if (!isField(world, cmd.entity)) {
          return false;
        }

        world.replace<dunya::objectmodel::Pose>(cmd.entity, cmd.newPose);

        return true;
      }

      else if constexpr (std::is_same_v<T, AddPrimitiveCommand>) {
        return world.insertPrimitive(
          cmd.entity,
          cmd.primitiveIndex,
          cmd.primitive
        );
      }

      else if constexpr (std::is_same_v<T, RemovePrimitiveCommand>) {
        return world.removePrimitive(cmd.entity, cmd.primitiveIndex);
      }

      else {
        static_assert(std::is_same_v<T, UpdatePrimitiveCommand>);

        return world.setPrimitive(
          cmd.entity,
          cmd.primitiveIndex,
          cmd.newPrimitive
        );
      }
    },
    command
  );
}

bool CommandHistory::revert(
  const Command& command,
  dunya::objectmodel::World& world
) {
  return std::visit(
    [&](const auto& cmd) -> bool {
      using T = std::decay_t<decltype(cmd)>;

      if constexpr (std::is_same_v<T, CreateFieldCommand>) {
        return world.destroyField(cmd.entity);
      }

      else if constexpr (std::is_same_v<T, DestroyFieldCommand>) {
        if (!cmd.grid.has_value() || !cmd.pose.has_value()) {
          return false;
        }

        if (!world.createFieldAt(cmd.entity, *cmd.pose, *cmd.grid)) {
          return false;
        }

        for (const auto& primitive : cmd.primitives) {
          if (!world.addPrimitive(cmd.entity, primitive)) {
            return false;
          }
        }

        return true;
      }

      else if constexpr (std::is_same_v<T, TransformFieldCommand>) {
        if (!isField(world, cmd.entity)) {
          return false;
        }

        world.replace<dunya::objectmodel::Pose>(cmd.entity, cmd.oldPose);

        return true;
      }

      else if constexpr (std::is_same_v<T, AddPrimitiveCommand>) {
        return world.removePrimitive(cmd.entity, cmd.primitiveIndex);
      }

      else if constexpr (std::is_same_v<T, RemovePrimitiveCommand>) {
        return world.insertPrimitive(
          cmd.entity,
          cmd.primitiveIndex,
          cmd.primitive
        );
      }

      else {
        static_assert(std::is_same_v<T, UpdatePrimitiveCommand>);

        return world.setPrimitive(
          cmd.entity,
          cmd.primitiveIndex,
          cmd.oldPrimitive
        );
      }
    },
    command
  );
}

}  // namespace dunya::editor
