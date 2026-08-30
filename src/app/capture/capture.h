#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/imagecompare/imagecompare.h>

#include <vulkan/vulkan.h>

namespace dunya::capture {

dunya::image::Bitmap read(
  const dunya::gpu::Device& device,
  VkImage image,
  VkImageLayout layout,
  VkExtent2D extent,
  VkFormat format
);

}
