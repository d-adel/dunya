#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>
#include <dunya/objectmodel/trait/assetbacked/assetbacked.h>

#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>

#include <cstdint>

namespace dunya::objectmodel {

struct Material {
  uint32_t index = UINT32_MAX;
};

template<>
inline constexpr bool selfContained<Material> = true;

template<>
inline constexpr bool authored<Material> = true;

template<>
inline constexpr bool assetBacked<Material> = true;

}
