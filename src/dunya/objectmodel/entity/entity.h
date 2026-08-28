#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace dunya::objectmodel {

// The one place EnTT is named. Not an id: an entity packs a slot and a version
// into 32 bits, so casting one to a subscript is wrong.
using Entity = entt::entity;

inline constexpr Entity INVALID_ENTITY = entt::null;

}  // namespace dunya::objectmodel
