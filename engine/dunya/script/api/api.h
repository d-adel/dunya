#pragma once

#include <cstdint>

#include <dunya/field/field.h>
#include <dunya/objectmodel/entity/entity.h>

namespace dunya::objectmodel {
class World;
}

namespace dunya::script {

extern "C" {

inline constexpr uint32_t API_VERSION = 6u;

struct FieldDescriptor {
  const char* name;
  uint32_t kind;
  uint32_t offset;
};

struct SdfEditDescriptor {
  uint32_t kind;
  uint32_t material;
  uint32_t operation;
  float blend;
  float position[3];
  float size[3];
  float rotation[4];
};

struct SdfDeformSummary {
  uint32_t cellsRemoved;
  float volumeRemoved;
  uint32_t brickBegin[3];
  uint32_t brickEnd[3];
  uint32_t sampleMinimum[3];
  uint32_t sampleExtent[3];
};

struct PhysicsVerbs {
  int32_t (*setRigidBody)(void* host, void* world, uint32_t entity, float mass);

  int32_t (*setVelocity)(
    void* host,
    void* world,
    uint32_t entity,
    const float* velocity
  );

  int32_t (*destroy)(void* host, void* world, uint32_t entity);
};

using SystemCallback = void (*)(
  void* user,
  void* world,
  void* input,
  float deltaSeconds,
  uint32_t frame
);

struct Api {
  uint32_t size;
  uint32_t version;

  uint32_t (*declareComponent)(
    void* world,
    const char* name,
    uint32_t size,
    const FieldDescriptor* fields,
    uint32_t fieldCount
  );

  uint32_t (*findComponent)(void* world, const char* name);

  int32_t (*setComponent)(
    void* world,
    uint32_t type,
    uint32_t entity,
    const void* value
  );

  void* (*getComponent)(void* world, uint32_t type, uint32_t entity);

  int32_t (*removeComponent)(void* world, uint32_t type, uint32_t entity);

  uint32_t (*componentCount)(void* world, uint32_t type);

  const uint32_t* (*componentEntities)(void* world, uint32_t type);

  void* (*componentData)(void* world, uint32_t type);

  int32_t (*deformSdf)(
    void* world,
    uint32_t entity,
    const SdfEditDescriptor* edit,
    SdfDeformSummary* result
  );

  uint32_t (*materialsUnderSdf)(
    void* world,
    uint32_t entity,
    const SdfEditDescriptor* edit,
    uint32_t* materials,
    uint32_t capacity
  );

  float (*sampleSdf)(void* world, uint32_t entity, const float* point);

  int32_t (*getPose)(void* world, uint32_t entity, float* pose);

  int32_t (*setPose)(void* world, uint32_t entity, const float* pose);

  uint32_t (*entities)(void* world, uint32_t* buffer, uint32_t capacity);

  int32_t (*hasComponent)(void* world, uint32_t entity, const char* name);

  int32_t (*bounds)(
    void* world,
    uint32_t entity,
    float* minimum,
    float* maximum
  );

  void (*log)(const char* message);

  int32_t (*addSystem)(
    void* schedule,
    int32_t order,
    const char* name,
    SystemCallback callback,
    void* user
  );

  int32_t (*keyHeld)(void* input, uint32_t key);

  int32_t (*keyPressed)(void* input, uint32_t key);

  int32_t (*keyReleased)(void* input, uint32_t key);

  int32_t (*mouseHeld)(void* input, uint32_t button);

  int32_t (*mousePressed)(void* input, uint32_t button);

  void (*cursor)(void* input, float* xy);

  uint32_t (*createSdfGrid)(
    void* world,
    const float* pose,
    const uint32_t* resolution,
    float margin
  );

  int32_t (*destroy)(void* world, uint32_t entity);

  int32_t (*addPrimitive)(
    void* world,
    uint32_t entity,
    const SdfEditDescriptor* shape
  );

  int32_t (*shareSdf)(void* world, uint32_t donor, uint32_t taker);

  uint32_t (*mainCamera)(void* world);

  int32_t (*setRigidBody)(void* world, uint32_t entity, float mass);

  int32_t (*setVelocity)(void* world, uint32_t entity, const float* velocity);

  int32_t (*screenPointToRay)(
    void* world,
    uint32_t camera,
    const float* screen,
    const float* viewport,
    float* ray
  );

  void (*viewport)(void* input, float* wh);
};
}

using LogSink = void (*)(const char* message);

void setLogSink(LogSink sink) noexcept;

void setPhysicsVerbs(const PhysicsVerbs* verbs, void* host) noexcept;

class PhysicsScope {
public:
  PhysicsScope() noexcept = default;

  PhysicsScope(const PhysicsVerbs* verbs, void* host) noexcept;

  PhysicsScope(const PhysicsScope&) = delete;
  PhysicsScope& operator=(const PhysicsScope&) = delete;

  PhysicsScope(PhysicsScope&& other) noexcept;
  PhysicsScope& operator=(PhysicsScope&& other) noexcept;

  ~PhysicsScope();

private:
  void restore() noexcept;

  const PhysicsVerbs* m_previous = nullptr;
  void* m_previousHost = nullptr;
  bool m_active = false;
};

[[nodiscard]] const Api& api() noexcept;

[[nodiscard]] dunya::field::Primitive primitiveFor(
  const dunya::objectmodel::World& world,
  dunya::objectmodel::Entity entity,
  const SdfEditDescriptor& edit
);

}
