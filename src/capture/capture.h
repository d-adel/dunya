#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/imagecompare/imagecompare.h>

#include <vulkan/vulkan.h>

// Reads a rendered image back into host memory. Slow and synchronous by design
// - nothing here belongs in a frame.
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
