#pragma once

#include "context/context.h"
#include "imagecompare/imagecompare.h"
#include "startupoptions/startupoptions.h"
#include "swapchain/swapchain.h"

#include <vulkan/vulkan.h>

#include <string>

/* What a harness run does with the frame it asked for.
 *
 * Reads the finished image back and then either writes it out or holds it
 * against a committed reference. Both are things a *test* wants from a
 * renderer, which is why they live here rather than growing inside the class
 * that owns the frame loop.
 */
class FrameCheck {
public:
  FrameCheck(
    const Context& context,
    const SwapChain& swapChain,
    const StartupOptions& options
  );

  FrameCheck(const FrameCheck&) = delete;
  FrameCheck& operator=(const FrameCheck&) = delete;
  FrameCheck(FrameCheck&&) = delete;
  FrameCheck& operator=(FrameCheck&&) = delete;

  ~FrameCheck() = default;

  // Whether this run asked for anything at all.
  bool wanted() const noexcept;

  // Reads the image and does what was asked. Call once, on a frame that
  // reached the point of being presented.
  void run(VkImage image);

  // Whether run() has happened, and whether a comparison found drift.
  bool ran() const noexcept;
  bool failed() const noexcept;

private:
  dunya::image::Bitmap read(VkImage image) const;
  bool compareToReference(const dunya::image::Bitmap& frame) const;

  const Context& m_context;
  const SwapChain& m_swapChain;

  std::string m_screenshot;
  std::string m_reference;

  bool m_ran = false;
  bool m_failed = false;
};
