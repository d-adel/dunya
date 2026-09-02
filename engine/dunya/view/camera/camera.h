#pragma once

#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/pose/pose.h>

#include <glm/glm.hpp>

namespace dunya::view {

class Camera {
public:
  Camera();

  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  ~Camera() = default;

  [[nodiscard]] dunya::objectmodel::Pose pose() const;

  [[nodiscard]] bool placed() const noexcept;

  void reset() noexcept;

  void placeFrom(const dunya::objectmodel::Pose& seat, float distance);

  void frame(const glm::vec3& centre, float radius);

  void orbit(float deltaYaw, float deltaPitch);

  void pan(float deltaX, float deltaY);

  void zoom(float delta);

  [[nodiscard]] glm::vec3 eye() const noexcept;

  [[nodiscard]] const dunya::objectmodel::Lens& lens() const noexcept;

private:
  void place(const glm::vec3& position, float yaw, float pitch);

  [[nodiscard]] glm::vec3 forward() const noexcept;

  [[nodiscard]] glm::vec3 pivot() const noexcept;

  void orbitAround(const glm::vec3& around);

  dunya::objectmodel::Lens m_lens{};

  glm::vec3 m_position;

  float m_pitch = 0.f;
  float m_yaw = 0.f;

  float m_distance = 10.0f;
  bool m_placed = false;
};

}
