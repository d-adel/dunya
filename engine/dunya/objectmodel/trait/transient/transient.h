#pragma once

namespace dunya::objectmodel {

template<typename T>
inline constexpr bool transient = false;

template<typename T>
concept Transient = transient<T>;

}
