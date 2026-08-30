#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/world/world.h>

#include <string>
#include <string_view>
#include <vector>

namespace dunya::serialize {

[[nodiscard]] std::vector<std::string_view> authoredComponentNames();

[[nodiscard]] bool readComponent(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity,
  std::string_view component,
  std::string& json
);

}
