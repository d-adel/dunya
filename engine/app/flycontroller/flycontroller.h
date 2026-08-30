#pragma once

#include <dunya/field/raycast/raycast.h>
#include <app/flycamera/flycamera.h>
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

  [[nodiscard]] FlyCamera& camera() noexcept;
  [[nodiscard]] const FlyCamera& camera() const noexcept;

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

  FlyCamera m_camera;
  FlyInput m_state{};

  bool m_looking = false;

  bool m_wasAccepting = false;
};
