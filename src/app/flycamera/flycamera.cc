#include "flycamera.ih"

FlyCamera::FlyCamera()
    : m_position(glm::vec3(0, 0, 2)), m_velocity(glm::vec3(0.f)) {}

void FlyCamera::place(const glm::vec3& position, float yaw, float pitch) {
  m_position = position;
  m_yaw = yaw;
  m_pitch = pitch;
  m_velocity = glm::vec3(0.0f);
}

void FlyCamera::update(float dt, FlyInput input) {
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
    glm::vec3(cameraRotation * glm::vec4(m_velocity * 0.9f, 0.f)) * dt;
}

glm::mat4 FlyCamera::viewMatrix() const {
  glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), m_position);
  glm::mat4 cameraRotation = rotationMatrix();
  return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 FlyCamera::rotationMatrix() const {
  glm::quat pitchRotation = glm::angleAxis(m_pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yawRotation = glm::angleAxis(m_yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void FlyCamera::setLens(const dunya::objectmodel::Lens& lens) noexcept {
  m_lens = lens;
}

const dunya::objectmodel::Lens& FlyCamera::lens() const noexcept {
  return m_lens;
}

glm::mat4 FlyCamera::projectionMatrix(float aspect) const {
  return dunya::objectmodel::projection(m_lens, aspect);
}

glm::vec4 FlyCamera::position() const {
  return glm::vec4(m_position, m_lens.nearPlane);
}
