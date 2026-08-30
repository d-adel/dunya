#include "meshrecordpacker.ih"

namespace dunya::renderer {

std::span<const MeshRecord> MeshRecordPacker::pack(
  const dunya::objectmodel::World& world,
  std::span<const dunya::objectmodel::Entity> entities
) {
  const entt::registry& registry = world.registry();

  m_records.clear();

  for (const dunya::objectmodel::Entity entity : entities) {
    m_records.push_back(
      {registry.get<dunya::objectmodel::Mesh>(entity).index,
       registry.get<dunya::objectmodel::Material>(entity).index,
       dunya::objectmodel::model(
         dunya::objectmodel::drawnPose(registry, entity)
       )}
    );
  }

  return m_records;
}

}
