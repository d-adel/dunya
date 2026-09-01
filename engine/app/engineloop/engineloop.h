#pragma once

#include <app/framecheck/framecheck.h>
#include <app/startupoptions/startupoptions.h>
#include <dunya/engine/engine/engine.h>
#include <dunya/objectmodel/worldquery/worldquery.h>
#include <dunya/platform/glfwlibrary/glfwlibrary.h>
#include <dunya/platform/input/input.h>
#include <dunya/platform/window/window.h>
#include <dunya/script/runner/runner.h>

class EngineLoop {
public:
  explicit EngineLoop(const StartupOptions& options);
  ~EngineLoop();

  EngineLoop(const EngineLoop&) = delete;
  EngineLoop& operator=(const EngineLoop&) = delete;
  EngineLoop(EngineLoop&&) = delete;
  EngineLoop& operator=(EngineLoop&&) = delete;

  int start(const StartupOptions& options = {});

private:
  void lookThrough(float aspect);

  void handleKeyEvent(const dunya::platform::KeyEvent& event);

  void handleMouseButtonEvent(const dunya::platform::MouseButtonEvent& event);

  void drawSky(VkCommandBuffer commands) const;

  [[nodiscard]] bool verifyBakes();

  dunya::platform::GLFWLibrary m_glfwLibrary;
  dunya::platform::Window m_window;

  dunya::engine::Engine m_engine;

  dunya::platform::Input m_input;

  dunya::script::Runner m_script;

  dunya::core::Telemetry m_telemetry;

  bool m_reportedMissingCamera = false;

  dunya::core::EventDispatcher::SubscriptionId m_keySubscription{};
  dunya::core::EventDispatcher::SubscriptionId m_mouseSubscription{};
};
