#pragma once

#include <dunya/core/asset/assetdatabase.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/trait/authoredcomponents/authoredcomponents.h>
#include <dunya/objectmodel/trait/assetbacked/assetbacked.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/directionallight/directionallight.h>
#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/maincamera/maincamera.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/mesh/mesh.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/component/environment/environment.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>
#include <dunya/objectmodel/dynamiccomponents/dynamiccomponents.h>
#include <dunya/objectmodel/world/world.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace dunya::serialize {

inline constexpr uint32_t WORLD_VERSION = 1u;

template<typename T>
using StoredType = std::
  conditional_t<dunya::objectmodel::assetBacked<T>, dunya::core::AssetId, T>;

struct StoredPrimitive {
  dunya::field::Primitive primitive{};
  dunya::core::AssetId material = dunya::core::INVALID_ASSET;
};

struct StoredField {
  std::string name;
  uint32_t kind = 0u;
  uint32_t offset = 0u;
};

struct StoredComponentType {
  std::string name;
  uint32_t size = 0u;
  std::vector<StoredField> fields;
};

struct StoredDynamic {
  std::string type;
  std::vector<double> values;
};

struct StoredEntity {
  std::optional<dunya::objectmodel::Pose> pose;
  std::optional<dunya::objectmodel::SdfGrid> grid;
  std::optional<dunya::objectmodel::Lens> lens;
  std::optional<dunya::objectmodel::MainCamera> mainCamera;
  std::optional<dunya::objectmodel::MassScale> massScale;
  std::optional<dunya::objectmodel::StaticBody> staticBody;
  std::optional<dunya::objectmodel::Deformable> deformable;

  std::optional<dunya::objectmodel::DirectionalLight> directionalLight;
  std::optional<dunya::objectmodel::Environment> environment;

  std::optional<dunya::core::AssetId> mesh;
  std::optional<dunya::core::AssetId> material;

  std::vector<StoredPrimitive> primitives;

  std::vector<StoredDynamic> dynamic;
};

template<dunya::objectmodel::Authored T>
struct PortableComponent {
  using Component = T;

  std::optional<StoredType<T>> StoredEntity::* slot;
};

inline constexpr auto PORTABLE_COMPONENTS = std::tuple{
  PortableComponent<dunya::objectmodel::Pose>{&StoredEntity::pose},
  PortableComponent<dunya::objectmodel::SdfGrid>{&StoredEntity::grid},
  PortableComponent<dunya::objectmodel::Lens>{&StoredEntity::lens},
  PortableComponent<dunya::objectmodel::MainCamera>{&StoredEntity::mainCamera},
  PortableComponent<dunya::objectmodel::MassScale>{&StoredEntity::massScale},
  PortableComponent<dunya::objectmodel::StaticBody>{&StoredEntity::staticBody},
  PortableComponent<dunya::objectmodel::Deformable>{&StoredEntity::deformable},
  PortableComponent<dunya::objectmodel::Mesh>{&StoredEntity::mesh},
  PortableComponent<dunya::objectmodel::Material>{&StoredEntity::material},
  PortableComponent<dunya::objectmodel::DirectionalLight>{
    &StoredEntity::directionalLight
  },
  PortableComponent<dunya::objectmodel::Environment>{&StoredEntity::environment}
};

template<typename... Ts>
consteval bool bindsEvery(dunya::objectmodel::ComponentList<Ts...>) {
  return std::apply(
    [](auto... bound) {
      return std::is_same_v<
        std::tuple<Ts...>,
        std::tuple<typename decltype(bound)::Component...>>;
    },
    PORTABLE_COMPONENTS
  );
}

static_assert(bindsEvery(dunya::objectmodel::AuthoredComponents{}));

struct StoredWorld {
  uint32_t version = WORLD_VERSION;
  std::vector<StoredEntity> entities;

  std::vector<StoredComponentType> componentTypes;
};

[[nodiscard]] StoredWorld captureWorld(
  const dunya::objectmodel::World& world,
  const dunya::core::AssetDatabase& assets
);

[[nodiscard]] bool restoreWorld(
  const StoredWorld& stored,
  dunya::objectmodel::World& world,
  const dunya::core::AssetDatabase& assets
);

[[nodiscard]] std::string writeWorld(const StoredWorld& stored);

[[nodiscard]] bool readWorld(std::string_view text, StoredWorld& stored);

}
