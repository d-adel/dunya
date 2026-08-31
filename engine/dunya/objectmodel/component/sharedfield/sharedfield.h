#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>

#include <dunya/field/sampledsdf/sampledsdf.h>

#include <memory>

namespace dunya::objectmodel {

struct SharedSdf {
  std::shared_ptr<dunya::field::SampledSdf> field;
};

template<>
inline constexpr bool transient<SharedSdf> = true;

}
