#pragma once

#include <dunya/field/raycast/raycast.h>
#include <dunya/viewport/camera/camera.h>
#include <dunya/platform/input/input.h>
#include <dunya/platform/window/window.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

class FlyController {
public:
  FlyController(dunya::platform::Input& input, dunya::platform::Window& window);

  FlyController(const FlyController&) = delete;
  FlyController& operator=(const FlyController&) = delete;
  FlyController(FlyController&&) = delete;
  FlyController& operator=(FlyController&&) = delete;

  [[nodiscard]] dunya::viewport::Camera& camera() noexcept;
  [[nodiscard]] const dunya::viewport::Camera& camera() const noexcept;

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

  dunya::viewport::Camera m_camera;
  dunya::viewport::FlyInput m_state{};

  bool m_looking = false;

  bool m_wasAccepting = false;
};
