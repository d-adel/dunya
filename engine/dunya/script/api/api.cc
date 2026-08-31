#include "api.ih"

namespace dunya::script {

namespace {

using dunya::objectmodel::ComponentSpec;
using dunya::objectmodel::ComponentType;
using dunya::objectmodel::Entity;
using dunya::objectmodel::FieldKind;
using dunya::objectmodel::FieldSpec;
using dunya::objectmodel::INVALID_COMPONENT_TYPE;
using dunya::objectmodel::World;

World* worldOf(void* handle) noexcept {
  return static_cast<World*>(handle);
}

Entity entityOf(uint32_t value) noexcept {
  return static_cast<Entity>(entt::entity{value});
}

uint32_t declareComponent(
  void* world,
  const char* name,
  uint32_t size,
  const FieldDescriptor* fields,
  uint32_t fieldCount
) {
  World* target = worldOf(world);

  if (target == nullptr || name == nullptr) {
    return INVALID_COMPONENT_TYPE;
  }

  ComponentSpec spec;
  spec.name = name;
  spec.size = size;

  for (uint32_t index = 0u; index < fieldCount; ++index) {
    const FieldDescriptor& field = fields[index];

    if (field.name == nullptr || !dunya::objectmodel::isFieldKind(field.kind)) {
      return INVALID_COMPONENT_TYPE;
    }

    spec.fields.push_back(
      FieldSpec{field.name, static_cast<FieldKind>(field.kind), field.offset}
    );
  }

  return target->dynamic().declare(std::move(spec));
}

uint32_t findComponent(void* world, const char* name) {
  World* target = worldOf(world);

  if (target == nullptr || name == nullptr) {
    return INVALID_COMPONENT_TYPE;
  }

  return target->dynamic().find(name);
}

int32_t setComponent(
  void* world,
  uint32_t type,
  uint32_t entity,
  const void* value
) {
  World* target = worldOf(world);

  if (target == nullptr || value == nullptr) {
    return 0;
  }

  const ComponentSpec* spec = target->dynamic().spec(type);

  if (spec == nullptr) {
    return 0;
  }

  const std::span<const std::byte> bytes(
    static_cast<const std::byte*>(value),
    spec->size
  );

  return target->dynamic().emplace(type, entityOf(entity), bytes) ? 1 : 0;
}

void* getComponent(void* world, uint32_t type, uint32_t entity) {
  World* target = worldOf(world);

  return target == nullptr ? nullptr
                           : target->dynamic().get(type, entityOf(entity));
}

int32_t removeComponent(void* world, uint32_t type, uint32_t entity) {
  World* target = worldOf(world);

  return target != nullptr && target->dynamic().remove(type, entityOf(entity))
           ? 1
           : 0;
}

uint32_t componentCount(void* world, uint32_t type) {
  World* target = worldOf(world);

  return target == nullptr
           ? 0u
           : static_cast<uint32_t>(target->dynamic().count(type));
}

const uint32_t* componentEntities(void* world, uint32_t type) {
  World* target = worldOf(world);

  if (target == nullptr) {
    return nullptr;
  }

  const std::span<const Entity> entities = target->dynamic().entities(type);

  return entities.empty() ? nullptr
                          : reinterpret_cast<const uint32_t*>(entities.data());
}

void* componentData(void* world, uint32_t type) {
  World* target = worldOf(world);

  if (target == nullptr) {
    return nullptr;
  }

  const std::span<std::byte> bytes = target->dynamic().data(type);

  return bytes.empty() ? nullptr : bytes.data();
}

LogSink g_sink = nullptr;

float sampleSdf(void* world, uint32_t entity, const float* point) {
  World* target = worldOf(world);

  if (target == nullptr || point == nullptr) {
    return 0.0f;
  }

  const Entity subject = entityOf(entity);
  const dunya::field::SampledSdf* field = target->sampledSdf(subject);

  if (
    field == nullptr
    || !target->registry().all_of<dunya::objectmodel::Pose>(subject)
  ) {
    return 0.0f;
  }

  const glm::mat4 toLocal = glm::inverse(
    dunya::objectmodel::model(
      target->registry().get<dunya::objectmodel::Pose>(subject)
    )
  );

  const glm::vec3 local =
    glm::vec3(toLocal * glm::vec4(point[0], point[1], point[2], 1.0f));

  return dunya::field::distance(*field, local);
}

int32_t getPose(void* world, uint32_t entity, float* pose) {
  World* target = worldOf(world);

  if (target == nullptr || pose == nullptr) {
    return 0;
  }

  const Entity subject = entityOf(entity);

  if (!target->registry().all_of<dunya::objectmodel::Pose>(subject)) {
    return 0;
  }

  const dunya::objectmodel::Pose& held =
    target->registry().get<dunya::objectmodel::Pose>(subject);

  pose[0] = held.position.x;
  pose[1] = held.position.y;
  pose[2] = held.position.z;
  pose[3] = held.rotation.x;
  pose[4] = held.rotation.y;
  pose[5] = held.rotation.z;
  pose[6] = held.rotation.w;

  return 1;
}

int32_t setPose(void* world, uint32_t entity, const float* pose) {
  World* target = worldOf(world);

  if (target == nullptr || pose == nullptr) {
    return 0;
  }

  const Entity subject = entityOf(entity);

  if (!target->registry().all_of<dunya::objectmodel::Pose>(subject)) {
    return 0;
  }

  dunya::objectmodel::Pose held{};
  held.position = glm::vec3(pose[0], pose[1], pose[2]);
  held.rotation = glm::quat(pose[6], pose[3], pose[4], pose[5]);

  target->replace<dunya::objectmodel::Pose>(subject, held);

  return 1;
}

uint32_t entities(void* world, uint32_t* buffer, uint32_t capacity) {
  World* target = worldOf(world);

  if (target == nullptr) {
    return 0u;
  }

  const std::vector<Entity> live = dunya::objectmodel::liveEntities(*target);

  for (uint32_t slot = 0u; slot < live.size() && slot < capacity; ++slot) {
    buffer[slot] = static_cast<uint32_t>(entt::to_integral(live[slot]));
  }

  return static_cast<uint32_t>(live.size());
}

int32_t hasComponent(void* world, uint32_t entity, const char* name) {
  World* target = worldOf(world);

  if (target == nullptr || name == nullptr) {
    return 0;
  }

  const std::vector<std::string> carried =
    dunya::objectmodel::componentNames(*target, entityOf(entity));

  return std::ranges::find(carried, name) == carried.end() ? 0 : 1;
}

int32_t bounds(void* world, uint32_t entity, float* minimum, float* maximum) {
  World* target = worldOf(world);

  if (target == nullptr || minimum == nullptr || maximum == nullptr) {
    return 0;
  }

  const dunya::objectmodel::WorldExtent extent =
    dunya::objectmodel::entityExtent(*target, entityOf(entity));

  if (extent.empty) {
    return 0;
  }

  for (uint32_t axis = 0u; axis < 3u; ++axis) {
    minimum[axis] = extent.minimum[axis];
    maximum[axis] = extent.maximum[axis];
  }

  return 1;
}

void logMessage(const char* message) {
  if (message == nullptr) {
    return;
  }

  if (g_sink != nullptr) {
    g_sink(message);
    return;
  }

  std::cout << "[script] " << message << std::endl;
}

size_t indexOf(
  const dunya::field::SampledSdf& field,
  const dunya::field::SampleBox& box,
  uint32_t x,
  uint32_t y,
  uint32_t z
) {
  const uint32_t px = box.minimum.x + x;
  const uint32_t py = box.minimum.y + y;
  const uint32_t pz = box.minimum.z + z;

  return static_cast<size_t>(pz) * field.resolution.y * field.resolution.x
         + static_cast<size_t>(py) * field.resolution.x + px;
}

SdfDeformNotify g_notify = nullptr;
void* g_notifyHost = nullptr;

}

dunya::field::Primitive primitiveFor(
  const World& world,
  Entity entity,
  const SdfEditDescriptor& edit
) {
  const dunya::objectmodel::Pose& pose =
    world.registry().get<dunya::objectmodel::Pose>(entity);

  const glm::mat4 toLocal = glm::inverse(dunya::objectmodel::model(pose));

  const glm::vec3 position(
    edit.position[0],
    edit.position[1],
    edit.position[2]
  );
  const glm::vec3 local = glm::vec3(toLocal * glm::vec4(position, 1.0f));

  const glm::quat wanted(
    edit.rotation[3],
    edit.rotation[0],
    edit.rotation[1],
    edit.rotation[2]
  );

  const glm::quat turn = glm::normalize(glm::conjugate(pose.rotation) * wanted);

  const float angle = glm::angle(turn);
  const glm::vec3 axis =
    angle > 1e-6f ? glm::axis(turn) : glm::vec3(0.0f, 1.0f, 0.0f);

  switch (edit.kind) {
    case 1u:
      return dunya::field::makeBox(
        local,
        glm::vec3(edit.size[0], edit.size[1], edit.size[2]),
        angle,
        axis,
        edit.material,
        edit.operation,
        edit.blend
      );

    case 2u:
      return dunya::field::makePlane(local, edit.material, edit.operation);

    case 3u:
      return dunya::field::makeCylinder(
        local,
        edit.size[0],
        edit.size[1],
        angle,
        axis,
        edit.material,
        edit.operation,
        edit.blend
      );

    default:
      return dunya::field::makeSphere(
        local,
        edit.size[0],
        edit.material,
        edit.operation,
        edit.blend
      );
  }
}

namespace {

int32_t deformSdf(
  void* world,
  uint32_t entity,
  const SdfEditDescriptor* edit,
  SdfDeformSummary* result
) {
  World* target = worldOf(world);

  if (target == nullptr || edit == nullptr) {
    return 0;
  }

  const Entity subject = entityOf(entity);

  if (
    !target->hasSampledSdf(subject)
    || !target->registry().all_of<dunya::objectmodel::Pose>(subject)
  ) {
    return 0;
  }

  const dunya::field::Primitive shape = primitiveFor(*target, subject, *edit);

  uint32_t removed = 0u;
  float cellVolume = 0.0f;
  dunya::field::WriteReport report{};

  try {
    target->patchSampledSdf(subject, [&](dunya::field::SampledSdf& field) {
      const dunya::field::SampleBox box = dunya::field::affectedBox(
        field,
        shape,
        dunya::field::DEFORM_BAND_VOXELS
      );

      std::vector<uint8_t> before(
        static_cast<size_t>(box.extent.x) * box.extent.y * box.extent.z,
        0u
      );

      size_t at = 0u;

      for (uint32_t z = 0u; z < box.extent.z; ++z) {
        for (uint32_t y = 0u; y < box.extent.y; ++y) {
          for (uint32_t x = 0u; x < box.extent.x; ++x) {
            const size_t index = indexOf(field, box, x, y, z);

            before[at++] = field.distances[index] < 0.0f ? 1u : 0u;
          }
        }
      }

      report = dunya::field::deformAndRepair(field, shape).write;

      at = 0u;

      for (uint32_t z = 0u; z < box.extent.z; ++z) {
        for (uint32_t y = 0u; y < box.extent.y; ++y) {
          for (uint32_t x = 0u; x < box.extent.x; ++x) {
            const size_t index = indexOf(field, box, x, y, z);

            if (before[at++] == 1u && field.distances[index] >= 0.0f) {
              ++removed;
            }
          }
        }
      }

      cellVolume = field.voxelSize.x * field.voxelSize.y * field.voxelSize.z;
    });
  } catch (const std::exception&) {
    return 0;
  }

  SdfDeformSummary filled{};
  filled.cellsRemoved = removed;
  filled.volumeRemoved = static_cast<float>(removed) * cellVolume;

  for (uint32_t axis = 0u; axis < 3u; ++axis) {
    filled.brickBegin[axis] = report.brickBegin[axis];
    filled.brickEnd[axis] = report.brickEnd[axis];
    filled.sampleMinimum[axis] = report.samples.minimum[axis];
    filled.sampleExtent[axis] = report.samples.extent[axis];
  }

  if (result != nullptr) {
    *result = filled;
  }

  if (g_notify != nullptr) {
    g_notify(g_notifyHost, entity, &filled);
  }

  return 1;
}

uint32_t materialsUnderSdf(
  void* world,
  uint32_t entity,
  const SdfEditDescriptor* edit,
  uint32_t* materials,
  uint32_t capacity
) {
  World* target = worldOf(world);

  if (target == nullptr || edit == nullptr) {
    return 0u;
  }

  const Entity subject = entityOf(entity);
  const dunya::field::SampledSdf* field = target->sampledSdf(subject);

  if (
    field == nullptr
    || !target->registry().all_of<dunya::objectmodel::Pose>(subject)
  ) {
    return 0u;
  }

  const dunya::field::Primitive shape = primitiveFor(*target, subject, *edit);

  dunya::field::Primitive probe = shape;
  probe.shapeConfig.z = dunya::core::FIELD_OP_UNION;

  const dunya::field::SampleBox box =
    dunya::field::affectedBox(*field, shape, 0u);

  std::vector<uint32_t> found;

  for (uint32_t z = 0u; z < box.extent.z; ++z) {
    for (uint32_t y = 0u; y < box.extent.y; ++y) {
      for (uint32_t x = 0u; x < box.extent.x; ++x) {
        const size_t index = indexOf(*field, box, x, y, z);

        if (field->distances[index] >= 0.0f) {
          continue;
        }

        const glm::vec3 at =
          glm::vec3(field->origin)
          + field->voxelSize * glm::vec3(box.minimum + glm::uvec3(x, y, z));

        if (dunya::field::sample({&probe, 1}, at).distance > 0.0f) {
          continue;
        }

        const uint32_t material = field->materials[index];

        if (std::ranges::find(found, material) == found.end()) {
          found.push_back(material);
        }
      }
    }
  }

  for (uint32_t slot = 0u; slot < found.size() && slot < capacity; ++slot) {
    materials[slot] = found[slot];
  }

  return static_cast<uint32_t>(found.size());
}

int32_t addSystem(
  void* schedule,
  int32_t order,
  const char* name,
  SystemCallback callback,
  void* user
) {
  auto* target = static_cast<dunya::systems::Schedule*>(schedule);

  if (target == nullptr || name == nullptr || callback == nullptr) {
    return 0;
  }

  const auto run = [callback, user](dunya::systems::Context& context) {
    callback(user, &context.world, context.deltaSeconds, context.frameIndex);
  };

  return target->add(order, name, run) ? 1 : 0;
}

constexpr Api TABLE{
  static_cast<uint32_t>(sizeof(Api)),
  API_VERSION,
  &declareComponent,
  &findComponent,
  &setComponent,
  &getComponent,
  &removeComponent,
  &componentCount,
  &componentEntities,
  &componentData,
  &deformSdf,
  &materialsUnderSdf,
  &sampleSdf,
  &getPose,
  &setPose,
  &entities,
  &hasComponent,
  &bounds,
  &logMessage,
  &addSystem
};

}

void setSdfDeformNotify(SdfDeformNotify notify, void* host) noexcept {
  g_notify = notify;
  g_notifyHost = host;
}

SdfDeformNotify sdfDeformNotify() noexcept {
  return g_notify;
}

void* sdfDeformHost() noexcept {
  return g_notifyHost;
}

SdfDeformScope::SdfDeformScope(SdfDeformNotify notify, void* host) noexcept
    : m_previous(g_notify), m_previousHost(g_notifyHost), m_active(true) {
  setSdfDeformNotify(notify, host);
}

SdfDeformScope::SdfDeformScope(SdfDeformScope&& other) noexcept
    : m_previous(other.m_previous),
      m_previousHost(other.m_previousHost),
      m_active(other.m_active) {
  other.m_active = false;
}

SdfDeformScope& SdfDeformScope::operator=(SdfDeformScope&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  restore();

  m_previous = other.m_previous;
  m_previousHost = other.m_previousHost;
  m_active = other.m_active;

  other.m_active = false;

  return *this;
}

SdfDeformScope::~SdfDeformScope() {
  restore();
}

void SdfDeformScope::restore() noexcept {
  if (!m_active) {
    return;
  }

  setSdfDeformNotify(m_previous, m_previousHost);

  m_active = false;
}

void setLogSink(LogSink sink) noexcept {
  g_sink = sink;
}

const Api& api() noexcept {
  return TABLE;
}

}
