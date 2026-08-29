#pragma once

#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/platform/input/input.h>
#include <dunya/platform/window/window.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

// Flying the camera, and the look mode that gates it.
//
// The Unity scene-view model: the cursor is visible and clickable by default,
// and only becomes a look control while the right button is held. That is one
// rule spread across four places - the key handler that only moves while
// looking, the mouse handler that only clicks while not, the cursor mode, and
// the delta the camera reads - so it is one object rather than four members on
// whatever owns the frame loop.
//
// It owns the Camera, because every reader of a camera wants the one being
// flown and there is exactly one.
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

  // The movement keys, and nothing else: returns false for a key it does not
  // own, so the caller's command handling stays the caller's.
  bool handleKey(const dunya::platform::KeyEvent& event, bool acceptsInput);

  // Once a frame, before the view matrix is read. `acceptsInput` is the window
  // focus and enabled gate, which belongs to whoever owns the window.
  void update(float dt, bool acceptsInput);

  void clear() noexcept;

  // The cursor as a world ray. Needs the cursor, the surface it is over and the
  // camera's matrices, which is why it cannot live with the editing it feeds.
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

  // Whether the last frame was accepting look input, so the delta can be
  // dropped across the edge rather than spinning the camera by a jump.
  bool m_wasAccepting = false;
};
