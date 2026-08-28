#pragma once

namespace dunya::objectmodel {

// Opt-in: writing this component needs no second write anywhere else. A
// component owning an arena range, a pool slot or state derived from another
// component is deliberately absent, and World::patch refuses it at compile
// time.
template<typename T>
inline constexpr bool selfContained = false;

template<typename T>
concept SelfContained = selfContained<T>;

}  // namespace dunya::objectmodel
