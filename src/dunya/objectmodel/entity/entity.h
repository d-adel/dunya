#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace dunya::objectmodel {

// The one place EnTT is named. Libraries below this one - field, gpu, platform,
// core - have no entities and do not link objectmodel, so they never see it.
//
// Deliberately not called an id. An entity packs a slot and a version into its
// 32 bits, so recycling changes the number and casting one to a subscript is
// wrong. Where a dense index is wanted, generate it while assembling the frame
// that needs it.
using Entity = entt::entity;

inline constexpr Entity INVALID_ENTITY = entt::null;

}  // namespace dunya::objectmodel
