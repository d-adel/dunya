#include "componentjson.ih"

namespace dunya::serialize {

std::vector<std::string_view> authoredComponentNames() {
  std::vector<std::string_view> names;

  dunya::objectmodel::AuthoredComponents::each([&]<typename T>() {
    names.push_back(dunya::objectmodel::componentName<T>());
  });

  return names;
}

bool readComponent(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity,
  std::string_view component,
  std::string& json
) {
  bool written = false;

  dunya::objectmodel::AuthoredComponents::each([&]<typename T>() {
    if (written || component != dunya::objectmodel::componentName<T>()) {
      return;
    }

    const auto& registry = world.registry();

    if (!registry.all_of<T>(entity)) {
      return;
    }

    if constexpr (std::is_empty_v<T>) {
      json = "{}";

      written = true;
    } else {
      written = !glz::write<glz::opts{.prettify = false}>(
        registry.get<T>(entity),
        json
      );
    }
  });

  return written;
}

}
