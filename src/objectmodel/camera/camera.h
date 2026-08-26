#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dunya::objectmodel {

struct CameraInput {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;

  float lookDx = 0.0f;
  float lookDy = 0.0f;

  // float rotateDx = 0.0f;
  // float rotateDy = 0.0f;

  // float zoomDelta = 0.0f;

  // float panDx = 0.0f;
  // float panDy = 0.0f;
};

class Camera {
public:
  static constexpr float nearPlane = 0.1f;
  static constexpr float farPlane = 10000.0f;

  Camera();

  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  ~Camera() = default;

  glm::mat4 viewMatrix() const;
  glm::mat4 rotationMatrix() const;
  glm::mat4 projectionMatrix(float aspect) const;
  glm::vec4 position() const;

  void update(float dt, CameraInput input);

private:
  glm::vec3 m_position;
  glm::vec3 m_velocity;

  float m_pitch = 0.f;
  float m_yaw = 0.f;
  float m_lookSensitivity = 1 / 200.f;
};

}  // namespace dunya::objectmodel
