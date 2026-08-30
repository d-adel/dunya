#pragma once

#include <dunya/field/sampledsdf/sampledsdf.h>

#include <memory>

namespace dunya::objectmodel {

struct SharedSdf {
  std::shared_ptr<dunya::field::SampledSdf> field;
};

}
