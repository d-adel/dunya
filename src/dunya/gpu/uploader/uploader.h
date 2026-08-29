#pragma once

#include <dunya/gpu/buffer/buffer.h>
#include <dunya/gpu/device/device.h>

#include <vulkan/vulkan.h>

#include <vector>

namespace dunya::gpu {

// Host-to-device copies that do not stop the frame.
//
// The alternative it replaces is OneShotCommand, which ends in
// vkQueueWaitIdle - a wait on *everything* the graphics queue holds, the two
// frames already in flight included. That is the right tool at load time and
// the wrong one during a frame: a volume upload measured at 13.6 ms was almost
// entirely the drain, and a single deformed object costs six of them, because
// a transition, a copy and a transition back are three submissions each.
//
// Here every copy in a frame goes into one command buffer, submitted once and
// never waited on. Ordering against the rendering that follows is free: the
// copies carry their own barriers, and a barrier orders against everything
// submitted earlier on the same queue, so the fragment shader that samples a
// volume this frame sees the write that preceded it in submission order.
//
// What that costs is the staging buffers, which the GPU is still reading after
// submit() returns. They are held until the batch's fence signals, checked
// without blocking.
class Uploader {
public:
  explicit Uploader(const Device& device);

  Uploader(const Uploader&) = delete;
  Uploader& operator=(const Uploader&) = delete;
  Uploader(Uploader&&) = delete;
  Uploader& operator=(Uploader&&) = delete;

  ~Uploader();

  // The command buffer this frame's copies record into, opened on first use.
  [[nodiscard]] VkCommandBuffer begin();

  // Hands over a staging buffer to hold until the GPU has read it.
  void keep(Buffer&& staging);

  // Submits whatever has been recorded since the last call. Does nothing when
  // nothing was, which is most frames.
  void submit();

  // Releases the batches whose fence has signalled. Cheap and non-blocking, so
  // the frame loop can call it unconditionally.
  void retire();

  // Whether anything has been recorded and not yet submitted.
  [[nodiscard]] bool pending() const noexcept;

private:
  // One submission's worth: the buffer it was recorded into, the staging the
  // GPU is reading from, and the fence that says when both are free.
  struct Batch {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::vector<Buffer> staging;
  };

  void release(Batch& batch) noexcept;

  const Device& m_device;
  VkCommandPool m_pool = VK_NULL_HANDLE;

  Batch m_open;
  bool m_recording = false;

  std::vector<Batch> m_inFlight;
};

}  // namespace dunya::gpu
