#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/imagecompare/imagecompare.h>

#include <vulkan/vulkan.h>

/* Reading a rendered image back into host memory.
 *
 * Deliberately ignorant of where the image came from. A swapchain image and an
 * offscreen colour attachment are both just an image, a layout and a format, so
 * this stays usable whichever one M25 ends up capturing - and the choice
 * between them becomes a choice of argument rather than a rewrite.
 *
 * Slow and synchronous by design: it waits for the copy. Nothing here belongs
 * in a frame.
 */
namespace dunya::capture {

// Copies the image into host memory as 8-bit RGBA, converting from the image's
// own channel order. The image is left in the layout it arrived in.
dunya::image::Bitmap read(
  const dunya::gpu::Device& device,
  VkImage image,
  VkImageLayout layout,
  VkExtent2D extent,
  VkFormat format
);

}  // namespace dunya::capture
