#include "capture.ih"

namespace dunya::capture {

namespace {

constexpr uint32_t CHANNELS = 4;

// Whether the format's first byte is blue rather than red. Presentable formats
// are usually BGRA and a PNG is RGBA, so getting this wrong does not fail - it
// silently produces an image with the red and blue swapped, which is exactly
// the kind of wrongness a golden image would then enshrine as correct.
bool blueFirst(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
      return false;

    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
      return true;

    default:
      throw std::runtime_error(
        "Cannot read back an image of this format: "
        + std::to_string(static_cast<int>(format))
      );
  }
}

VkImageMemoryBarrier2 transition(
  VkImage image,
  VkImageLayout from,
  VkImageLayout to
) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  return barrier;
}

void submitBarrier(VkCommandBuffer cmd, const VkImageMemoryBarrier2& barrier) {
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(cmd, &dependency);
}

}  // namespace

dunya::image::Bitmap read(
  const dunya::gpu::Device& device,
  VkImage image,
  VkImageLayout layout,
  VkExtent2D extent,
  VkFormat format
) {
  const bool swapRedAndBlue = blueFirst(format);

  const VkDeviceSize bytes =
    static_cast<VkDeviceSize>(extent.width) * extent.height * CHANNELS;

  dunya::gpu::Buffer readback(
    device,
    bytes,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  dunya::gpu::OneShotCommand cmd;
  cmd.start(device);

  submitBarrier(
    cmd.cmdBuffer(),
    transition(image, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
  );

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {extent.width, extent.height, 1};

  vkCmdCopyImageToBuffer(
    cmd.cmdBuffer(),
    image,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    readback.buffer(),
    1,
    &region
  );

  // Back to where it started. A swapchain image handed back in the wrong layout
  // would be presented from a layout the driver was not promised.
  submitBarrier(
    cmd.cmdBuffer(),
    transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout)
  );

  cmd.submit(device);

  dunya::image::Bitmap bitmap;
  bitmap.width = extent.width;
  bitmap.height = extent.height;
  bitmap.pixels.resize(static_cast<size_t>(bytes));

  void* mapped = nullptr;
  if (vkMapMemory(device.vkDevice(), readback.memory(), 0, bytes, 0, &mapped)
      != VK_SUCCESS) {
    throw std::runtime_error("Failed to map the capture readback buffer");
  }

  std::memcpy(bitmap.pixels.data(), mapped, static_cast<size_t>(bytes));
  vkUnmapMemory(device.vkDevice(), readback.memory());

  if (swapRedAndBlue) {
    for (size_t at = 0; at < bitmap.pixels.size(); at += CHANNELS) {
      std::swap(bitmap.pixels[at], bitmap.pixels[at + 2]);
    }
  }

  return bitmap;
}

}  // namespace dunya::capture
