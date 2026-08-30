#pragma once

#include <dunya/objectmodel/component/lens/lens.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct FlyInput {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;

  float lookDx = 0.0f;
  float lookDy = 0.0f;
};

class FlyCamera {
public:
  FlyCamera();

  FlyCamera(const FlyCamera&) = delete;
  FlyCamera& operator=(const FlyCamera&) = delete;

  ~FlyCamera() = default;

  glm::mat4 viewMatrix() const;
  glm::mat4 rotationMatrix() const;
  glm::mat4 projectionMatrix(float aspect) const;
  glm::vec4 position() const;

  void update(float dt, FlyInput input);

  void place(const glm::vec3& position, float yaw, float pitch);

  [[nodiscard]] const dunya::objectmodel::Lens& lens() const noexcept;

private:
  dunya::objectmodel::Lens m_lens{};

  glm::vec3 m_position;
  glm::vec3 m_velocity;

  float m_pitch = 0.f;
  float m_yaw = 0.f;
  float m_lookSensitivity = 1 / 200.f;
};
