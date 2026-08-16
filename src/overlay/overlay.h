#pragma once

#include "context/context.h"
#include "swapchain/swapchain.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/* The development overlay.
 *
 * A tool, not part of the picture: nothing the renderer draws depends on it,
 * and a run that captures a frame simply never builds one.
 *
 * The frame is split the way the rest of the project already splits one -
 * Application decides *what* to show, Renderer decides *when* it is recorded -
 * so building the widgets and submitting their geometry are separate calls.
 * ImGui::Render() happens at the end of building rather than at recording, so a
 * frame whose swapchain went stale still closes the ImGui frame it opened.
 */
class Overlay {
public:
  Overlay(const Context& context, const SwapChain& swapChain);

  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;
  Overlay(Overlay&&) = delete;
  Overlay& operator=(Overlay&&) = delete;

  ~Overlay();

  /* Registers a panel. The callback draws it with ImGui calls of its own.
   *
   * The overlay never learns what is inside one, which is the whole point: a
   * panel belongs next to the data it shows, so whoever owns the data writes
   * it, and adding one costs nothing here. The alternative - a method per
   * panel, like the hardcoded stats window this replaced - makes the overlay
   * grow a dependency on every subsystem in the project.
   */
  void panel(std::string name, std::function<void()> draw);

  // Opens a frame, fills it from the registered panels, and closes it. Always
  // paired, whatever the swapchain does afterwards.
  void begin();
  void build();
  void end();

  // Submits the geometry the calls above produced.
  void record(VkCommandBuffer commandBuffer) const;

  // Whether the cursor or the keyboard currently belongs to the overlay. A
  // click that lands on a slider must not also carve the world behind it.
  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  struct Panel {
    std::string name;
    std::function<void()> draw;
    bool visible = true;
  };

  const Context& m_context;

  std::vector<Panel> m_panels;

  // ImGui manages its own descriptors and wants a pool handle rather than this
  // project's DescriptorGroup. Forcing the abstraction on it would bend
  // DescriptorGroup toward a case it was never for.
  VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
