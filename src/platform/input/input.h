#pragma once

#include "core/event/event.h"

#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <unordered_map>

enum class KeyEventType {
  Pressed,
  Released,
  SinglePressed,
  DoublePressed,
  Hold,
  Repeat
};

struct KeyEvent {
  int key;
  KeyEventType type;
};

enum class MouseButtonEventType {
  Pressed,
  Released
};

struct MouseButtonEvent {
  int button;
  MouseButtonEventType type;
  int mods;
};

struct KeyTiming {
  uint64_t doubleWindowMs = 250;    // Max gap between 2 presses
  uint64_t holdDelayMs = 350;       // Time before Hold event
  uint64_t repeatDelayMs = 400;     // Time before first Repeat
  uint64_t repeatIntervalMs = 100;  // Repeat cadence
};

struct KeyState {
  bool prevDown = false;

  bool pendingSingle = false;
  uint64_t lastPressMs = 0;

  bool holdFired = false;
  bool repeatArmed = false;
  uint64_t pressedAtMs = 0;

  uint64_t nextRepeatMs = 0;
};

struct Cursor {
  double x, y, dx, dy;
};

class Input {
public:
  explicit Input(GLFWwindow* window, KeyTiming timing = {});

  ~Input();

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;
  Input(Input&& other) noexcept = delete;
  Input& operator=(Input&& other) noexcept = delete;

  bool enabled() const noexcept;
  void toggleEnabled();

  void update();

  [[nodiscard]]
  bool isDown(int key) const noexcept;

  void setTiming(const KeyTiming& timing) noexcept;

  Cursor cursor() const noexcept;
  void cursorDeltaInvalid();

private:
  static void keyCallback(
    GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mods
  );

  static void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods
  );

  void updateCursor();
  void handleKey(int key, int action);
  void dispatch(int key, KeyEventType type);

  [[nodiscard]]
  static std::uint64_t currentTimeMs();

  [[nodiscard]]
  static bool isValidKey(int key) noexcept;

  bool m_enabled;
  bool m_cursorDeltaInvalid;

  GLFWwindow* m_window = nullptr;
  KeyTiming m_timing{};

  std::array<KeyState, GLFW_KEY_LAST + 1> m_keyStates{};

  GLFWkeyfun m_previousKeyCallback = nullptr;
  GLFWmousebuttonfun m_previousMouseButtonCallback = nullptr;

  Cursor m_cursor{};

  inline static std::unordered_map<GLFWwindow*, Input*> s_inputs;
};
