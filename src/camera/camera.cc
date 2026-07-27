#include "camera.ih"

Camera::Camera() : m_position(glm::vec3(0, 0, 5)), m_velocity(glm::vec3(0.f)) {}

void Camera::update(float dt, CameraInput input) {
  m_velocity.z = (float)(input.back - input.forward);
  m_velocity.x = (float)(input.right - input.left);
  m_velocity.y = (float)(input.up - input.down);
  m_yaw += input.lookDx * m_lookSensitivity;
  m_pitch = glm::clamp(
    m_pitch - input.lookDy * m_lookSensitivity,
    glm::radians(-89.0f),
    glm::radians(89.0f)
  );

  glm::mat4 cameraRotation = rotationMatrix();
  m_position +=
    glm::vec3(cameraRotation * glm::vec4(m_velocity * 0.5f, 0.f)) * dt;
}

glm::mat4 Camera::viewMatrix() const {
  glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), m_position);
  glm::mat4 cameraRotation = rotationMatrix();
  return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::rotationMatrix() const {
  glm::quat pitchRotation = glm::angleAxis(m_pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yawRotation = glm::angleAxis(m_yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
  glm::mat4 projection =
    glm::perspective(glm::radians(70.f), aspect, 0.1f, 10000.f);

  projection[1][1] *= -1;

  return projection;
}
