#pragma once

#include <dunya/gpu/context/context.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// A tool, not part of the picture: nothing the renderer draws depends on it.
// Building the widgets and submitting them are separate calls, so a stale
// swapchain still closes the ImGui frame it opened.
class Overlay {
public:
  Overlay(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain
  );

  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;
  Overlay(Overlay&&) = delete;
  Overlay& operator=(Overlay&&) = delete;

  ~Overlay();

  // Registers a panel. The callback draws it, so a panel lives next to the data
  // it shows and the overlay depends on no subsystem.
  void panel(std::string name, std::function<void()> draw);

  // A message drawn centred over everything, for work that blocks the loop
  // long enough to look like a hang. Empty shows nothing.
  void notice(std::string text);

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
  // Separate from build so the panel loop keeps one job and the notice is
  // drawn last, over everything.
  void drawNotice();

  struct Panel {
    std::string name;
    std::function<void()> draw;
    bool visible = true;
  };

  const dunya::gpu::Context& m_context;

  std::vector<Panel> m_panels;

  // ImGui manages its own descriptors and wants a pool handle rather than this
  // project's DescriptorGroup. Forcing the abstraction on it would bend
  // DescriptorGroup toward a case it was never for.
  std::string m_notice;

  VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
