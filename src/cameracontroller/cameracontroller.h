#pragma once

#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/platform/input/input.h>
#include <dunya/platform/window/window.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

class CameraController {
public:
  CameraController(
    dunya::platform::Input& input,
    dunya::platform::Window& window
  );

  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;
  CameraController(CameraController&&) = delete;
  CameraController& operator=(CameraController&&) = delete;

  [[nodiscard]] dunya::objectmodel::Camera& camera() noexcept;
  [[nodiscard]] const dunya::objectmodel::Camera& camera() const noexcept;

  [[nodiscard]] bool looking() const noexcept;

  void setLookMode(bool looking);

  bool handleKey(const dunya::platform::KeyEvent& event, bool acceptsInput);

  void update(float dt, bool acceptsInput);

  void clear() noexcept;

  [[nodiscard]] dunya::field::Ray cursorRay(
    VkExtent2D extent,
    const glm::mat4& viewProjection
  ) const;

private:
  dunya::platform::Input& m_input;
  dunya::platform::Window& m_window;

  dunya::objectmodel::Camera m_camera;
  dunya::objectmodel::CameraInput m_state{};

  bool m_looking = false;

  bool m_wasAccepting = false;
};
