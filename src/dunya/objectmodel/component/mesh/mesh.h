#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>
#include <dunya/objectmodel/trait/assetbacked/assetbacked.h>

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

struct Mesh {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Mesh> = true;

template<>
inline constexpr bool authored<Mesh> = true;

template<>
inline constexpr bool assetBacked<Mesh> = true;

}
