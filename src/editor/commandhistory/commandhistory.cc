#include "commandhistory.ih"

#include <type_traits>
#include <utility>

namespace dunya::editor {

void CommandHistory::undo(dunya::objectmodel::ObjectRegistry& registry) {
  if (m_undo.empty()) {
    return;
  }

  Command command = std::move(m_undo.back());
  m_undo.pop_back();

  if (!revert(command, registry)) {
    m_undo.push_back(std::move(command));
    return;
  }

  m_redo.push_back(std::move(command));
}

void CommandHistory::redo(dunya::objectmodel::ObjectRegistry& registry) {
  if (m_redo.empty()) {
    return;
  }

  Command command = std::move(m_redo.back());
  m_redo.pop_back();

  if (!apply(command, registry)) {
    m_redo.push_back(std::move(command));
    return;
  }

  m_undo.push_back(std::move(command));
}

void CommandHistory::clear() {
  m_undo.clear();
  m_redo.clear();
}

bool CommandHistory::apply(
  Command& command,
  dunya::objectmodel::ObjectRegistry& registry
) {
  return std::visit(
    [&](auto& cmd) -> bool {
      using T = std::decay_t<decltype(cmd)>;

      if constexpr (std::is_same_v<T, AddFieldObjectCommand>) {
        if (cmd.id == dunya::core::INVALID_OBJECT_ID) {
          cmd.id = registry.addFieldObject(cmd.object);
          return cmd.id != dunya::core::INVALID_OBJECT_ID;
        }

        return registry.addFieldObjectAt(cmd.id, cmd.object);
      }

      else if constexpr (std::is_same_v<T, RemoveFieldObjectCommand>) {
        if (!cmd.object.has_value()) {
          if (!registry.contains(cmd.id)) {
            return false;
          }

          cmd.object = registry.getFieldObject(cmd.id);

          const auto primitives = registry.getPrimitives(cmd.id);

          cmd.primitives.assign(primitives.begin(), primitives.end());
        }

        return registry.removeFieldObject(cmd.id);
      }

      else if constexpr (std::is_same_v<T, TransformFieldObjectCommand>) {
        if (!registry.contains(cmd.id)) {
          return false;
        }

        dunya::objectmodel::FieldObject& object =
          registry.getFieldObject(cmd.id);

        object.position = cmd.newPosition;
        object.rotation = cmd.newRotation;

        return true;
      }

      else if constexpr (std::is_same_v<T, AddPrimitiveCommand>) {
        return registry.insertPrimitive(
          cmd.objectId,
          cmd.primitiveIndex,
          cmd.primitive
        );
      }

      else if constexpr (std::is_same_v<T, RemovePrimitiveCommand>) {
        return registry.removePrimitive(cmd.objectId, cmd.primitiveIndex);
      }

      else {
        static_assert(std::is_same_v<T, UpdatePrimitiveCommand>);

        return registry.setPrimitive(
          cmd.objectId,
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
  dunya::objectmodel::ObjectRegistry& registry
) {
  return std::visit(
    [&](const auto& cmd) -> bool {
      using T = std::decay_t<decltype(cmd)>;

      if constexpr (std::is_same_v<T, AddFieldObjectCommand>) {
        return registry.removeFieldObject(cmd.id);
      }

      else if constexpr (std::is_same_v<T, RemoveFieldObjectCommand>) {
        if (!cmd.object.has_value()) {
          return false;
        }

        if (!registry.addFieldObjectAt(cmd.id, *cmd.object)) {
          return false;
        }

        for (const auto& primitive : cmd.primitives) {
          if (!registry.addPrimitive(cmd.id, primitive)) {
            return false;
          }
        }

        return true;
      }

      else if constexpr (std::is_same_v<T, TransformFieldObjectCommand>) {
        if (!registry.contains(cmd.id)) {
          return false;
        }

        dunya::objectmodel::FieldObject& object =
          registry.getFieldObject(cmd.id);

        object.position = cmd.oldPosition;
        object.rotation = cmd.oldRotation;

        return true;
      }

      else if constexpr (std::is_same_v<T, AddPrimitiveCommand>) {
        return registry.removePrimitive(cmd.objectId, cmd.primitiveIndex);
      }

      else if constexpr (std::is_same_v<T, RemovePrimitiveCommand>) {
        return registry.insertPrimitive(
          cmd.objectId,
          cmd.primitiveIndex,
          cmd.primitive
        );
      }

      else {
        static_assert(std::is_same_v<T, UpdatePrimitiveCommand>);

        return registry.setPrimitive(
          cmd.objectId,
          cmd.primitiveIndex,
          cmd.oldPrimitive
        );
      }
    },
    command
  );
}

}  // namespace dunya::editor
