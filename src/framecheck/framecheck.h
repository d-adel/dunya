#pragma once

#include <dunya/gpu/context/context.h>
#include <dunya/imagecompare/imagecompare.h>
#include <startupoptions/startupoptions.h>
#include <dunya/gpu/swapchain/swapchain.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <fstream>
#include <string>

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

  bool wanted() const noexcept;

  bool capturing() const noexcept;

  double lastCaptureMs() const noexcept;

  void run(VkImage image);

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

  std::ofstream m_stream;
  uint32_t m_width = 0;
  uint32_t m_height = 0;

  std::string m_screenshot;
  std::string m_reference;

  bool m_ran = false;
  bool m_failed = false;
};
