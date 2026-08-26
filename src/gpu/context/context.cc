#include "context.ih"

Context::Context()
    : m_instance(validationLayers),
      m_surface(m_instance.handle(), m_window.handle()),
      m_device(m_instance.handle(), m_surface.handle()) {}

const Window& Context::window() const noexcept {
  return m_window;
}

Window& Context::window() {
  return m_window;
}

const Instance& Context::instance() const noexcept {
  return m_instance;
}

const Surface& Context::surface() const noexcept {
  return m_surface;
}

const Device& Context::device() const noexcept {
  return m_device;
}

Device& Context::device() {
  return m_device;
}
