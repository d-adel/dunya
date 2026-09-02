#include "lookthrough.ih"

namespace dunya::view {

bool bindingIsLive(
  const Viewport& port,
  const dunya::objectmodel::World& world
) {
  if (port.camera == dunya::objectmodel::INVALID_ENTITY) {
    return false;
  }

  return world.registry()
    .all_of<dunya::objectmodel::Pose, dunya::objectmodel::Lens>(port.camera);
}

dunya::objectmodel::CameraView lookThrough(
  const Viewport& port,
  const dunya::objectmodel::World& world,
  float aspect
) {
  if (!bindingIsLive(port, world)) {
    return dunya::objectmodel::cameraView(port.pose, port.lens, aspect);
  }

  const entt::registry& registry = world.registry();

  return dunya::objectmodel::cameraView(
    registry.get<const dunya::objectmodel::Pose>(port.camera),
    registry.get<const dunya::objectmodel::Lens>(port.camera),
    aspect
  );
}

}
