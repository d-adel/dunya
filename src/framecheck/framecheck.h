#pragma once

#include <dunya/gpu/context/context.h>
#include <dunya/imagecompare/imagecompare.h>
#include <startupoptions/startupoptions.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <fstream>
#include <string>

// Reads the finished frame back, then writes it out or holds it against a
// committed reference. Both are things a test wants, not the frame loop.
class FrameCheck {
public:
  FrameCheck(
    const dunya::gpu::Context& context,
    const dunya::gpu::SwapChain& swapChain,
    const StartupOptions& options
  );

  FrameCheck(const FrameCheck&) = delete;
  FrameCheck& operator=(const FrameCheck&) = delete;
  FrameCheck(FrameCheck&&) = delete;
  FrameCheck& operator=(FrameCheck&&) = delete;

  ~FrameCheck();

  // Whether this run asked for anything at all.
  bool wanted() const noexcept;

  // Recording, which is not a test: the loop keeps running and the frames are
  // numbered. Kept apart from wanted() because a capture run wants the demo
  // playing, and a test run wants the authored scene.
  bool capturing() const noexcept;

  // Milliseconds the last run() spent reading the frame back and encoding it.
  // The frame loop subtracts this before reporting, because a recording that
  // showed its own encoder cost as the frame rate would be advertising the
  // wrong number.
  double lastCaptureMs() const noexcept;

  // Reads the image and does what was asked. Call once, on a frame that
  // reached the point of being presented.
  void run(VkImage image);

  // Whether run() has happened, and whether a comparison found drift.
  bool ran() const noexcept;
  bool failed() const noexcept;

private:
  dunya::image::Bitmap read(VkImage image) const;
  [[nodiscard]]
  bool compareToReference(const dunya::image::Bitmap& frame) const;

  const dunya::gpu::Context& m_context;
  const dunya::gpu::SwapChain& m_swapChain;

  std::string m_capture;
  uint32_t m_captured = 0;
  double m_lastCaptureMs = 0.0;

  // One file for the whole recording, opened on the first frame so its size
  // comes from the swap chain rather than being guessed.
  std::ofstream m_stream;
  uint32_t m_width = 0;
  uint32_t m_height = 0;

  std::string m_screenshot;
  std::string m_reference;

  bool m_ran = false;
  bool m_failed = false;
};
