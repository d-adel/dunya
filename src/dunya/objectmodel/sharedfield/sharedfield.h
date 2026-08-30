#pragma once

#include <dunya/field/sampled/sampled.h>

#include <memory>

namespace dunya::objectmodel {

struct SharedField {
  std::shared_ptr<dunya::field::SampledField> field;
};

}
