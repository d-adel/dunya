#include "descriptorgroup.ih"

DescriptorGroup::DescriptorGroup(
  const Device& device,
  uint32_t frameCount,
  std::vector<BufferBinding> buffers,
  std::vector<ImageBinding> images
)
    : m_device(device.vkDevice()), m_frameCount(frameCount) {
  createSetLayout(buffers, images);
  createBuffers(device, buffers);
  createPool(buffers.size(), images.size());
  createSets(images);
}

DescriptorGroup::~DescriptorGroup() {
  vkDestroyDescriptorPool(m_device, m_pool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
}

const VkDescriptorSetLayout& DescriptorGroup::setLayout() const noexcept {
  return m_setLayout;
}

const VkDescriptorSet& DescriptorGroup::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_sets.at(frame);
}

void DescriptorGroup::write(
  uint32_t binding,
  uint32_t frame,
  const void* data,
  size_t size
) {
  for (Slot& slot : m_slots) {
    if (slot.binding != binding) {
      continue;
    }

    assert(size <= slot.size);
    memcpy(slot.mapped.at(slot.perFrame ? frame : 0), data, size);

    return;
  }

  throw std::runtime_error("Write to a binding this group does not own");
}

void DescriptorGroup::createSetLayout(
  const std::vector<BufferBinding>& buffers,
  const std::vector<ImageBinding>& images
) {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(buffers.size() + images.size());

  for (const BufferBinding& buffer : buffers) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = buffer.binding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = buffer.stages;
    layoutBinding.pImmutableSamplers = nullptr;

    bindings.push_back(layoutBinding);
  }

  for (const ImageBinding& image : images) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = image.binding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = image.stages;
    layoutBinding.pImmutableSamplers = nullptr;

    bindings.push_back(layoutBinding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (
    vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_setLayout)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create descriptor set layout");
  }
}

void DescriptorGroup::createBuffers(
  const Device& device,
  const std::vector<BufferBinding>& buffers
) {
  m_slots.reserve(buffers.size());

  for (const BufferBinding& binding : buffers) {
    const uint32_t count = binding.perFrame ? m_frameCount : 1;

    Slot slot;
    slot.binding = binding.binding;
    slot.perFrame = binding.perFrame;
    slot.size = binding.size;
    slot.buffers.reserve(count);
    slot.mapped.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
      slot.buffers.emplace_back(
        device,
        binding.size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      );

      if (
        vkMapMemory(
          m_device,
          slot.buffers[i].memory(),
          0,
          binding.size,
          0,
          &slot.mapped[i]
        )
        != VK_SUCCESS
      ) {
        throw std::runtime_error("Failed to map uniform buffer memory");
      }
    }

    m_slots.push_back(std::move(slot));
  }
}

void DescriptorGroup::createPool(size_t bufferCount, size_t imageCount) {
  std::vector<VkDescriptorPoolSize> poolSizes;

  if (bufferCount > 0) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount =
      static_cast<uint32_t>(bufferCount) * m_frameCount;

    poolSizes.push_back(poolSize);
  }

  if (imageCount > 0) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = static_cast<uint32_t>(imageCount) * m_frameCount;

    poolSizes.push_back(poolSize);
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = m_frameCount;

  if (
    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_pool) != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create descriptor pool");
  }
}

void DescriptorGroup::createSets(const std::vector<ImageBinding>& images) {
  const std::vector<VkDescriptorSetLayout> layouts(m_frameCount, m_setLayout);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_pool;
  allocInfo.descriptorSetCount = m_frameCount;
  allocInfo.pSetLayouts = layouts.data();

  m_sets.resize(m_frameCount);

  if (
    vkAllocateDescriptorSets(m_device, &allocInfo, m_sets.data()) != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to allocate descriptor sets");
  }

  for (uint32_t frame = 0; frame < m_frameCount; ++frame) {
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> writes;

    bufferInfos.reserve(m_slots.size());
    imageInfos.reserve(images.size());
    writes.reserve(m_slots.size() + images.size());

    for (const Slot& slot : m_slots) {
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = slot.buffers.at(slot.perFrame ? frame : 0).buffer();
      bufferInfo.offset = 0;
      bufferInfo.range = slot.size;

      bufferInfos.push_back(bufferInfo);

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_sets[frame];
      write.dstBinding = slot.binding;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write.descriptorCount = 1;
      write.pBufferInfo = &bufferInfos.back();

      writes.push_back(write);
    }

    for (const ImageBinding& image : images) {
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = image.view;
      imageInfo.sampler = image.sampler;

      imageInfos.push_back(imageInfo);

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_sets[frame];
      write.dstBinding = image.binding;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.descriptorCount = 1;
      write.pImageInfo = &imageInfos.back();

      writes.push_back(write);
    }

    vkUpdateDescriptorSets(
      m_device,
      static_cast<uint32_t>(writes.size()),
      writes.data(),
      0,
      nullptr
    );
  }
}
