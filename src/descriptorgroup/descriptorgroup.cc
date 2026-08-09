#include "descriptorgroup.ih"

DescriptorGroup::DescriptorGroup(
  const Device& device,
  uint32_t frameCount,
  std::vector<BufferBinding> buffers,
  std::vector<ImageBinding> images
)
    : m_device(device.vkDevice()), m_frameCount(frameCount) {
  for (const auto& image : images) {
    if (image.elements.size() > image.capacity) {
      throw std::runtime_error(
        "Number of image bindings larger than the current capacity"
      );
    }
  }
  createSetLayout(buffers, images);
  createBuffers(device, buffers);
  createPool(buffers.size(), images);
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
  std::vector<VkDescriptorBindingFlags> bindingFlags;
  bindings.reserve(buffers.size() + images.size());
  bindingFlags.reserve(buffers.size() + images.size());

  for (const BufferBinding& buffer : buffers) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = buffer.binding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = buffer.stages;
    layoutBinding.pImmutableSamplers = nullptr;

    bindings.push_back(layoutBinding);
    bindingFlags.push_back(0);
  }

  for (const ImageBinding& image : images) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = image.binding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = image.capacity;
    layoutBinding.stageFlags = image.stages;
    layoutBinding.pImmutableSamplers = nullptr;

    bindings.push_back(layoutBinding);
    bindingFlags.push_back(
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    );
  }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
  bindingFlagsInfo.sType =
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlagsInfo.pBindingFlags = bindingFlags.data();
  bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());

  bool hasUpdateAfterBindBit = std::any_of(
    bindingFlags.begin(),
    bindingFlags.end(),
    [](VkDescriptorBindingFlags n) {
      return (n & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0;
    }
  );

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  layoutInfo.flags =
    hasUpdateAfterBindBit
      ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
      : 0;
  layoutInfo.pNext = &bindingFlagsInfo;

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

void DescriptorGroup::createPool(
  size_t bufferCount,
  const std::vector<ImageBinding>& images
) {
  std::vector<VkDescriptorPoolSize> poolSizes;

  if (bufferCount > 0) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount =
      static_cast<uint32_t>(bufferCount) * m_frameCount;

    poolSizes.push_back(poolSize);
  }

  size_t totalCapacity = 0;
  for (const auto& item : images) {
    totalCapacity += item.capacity;
  }

  if (totalCapacity > 0) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount =
      static_cast<uint32_t>(totalCapacity) * m_frameCount;
    poolSizes.push_back(poolSize);
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = m_frameCount;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

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

  size_t totalCapacity = 0;
  for (const auto& item : images) {
    totalCapacity += item.capacity;
  }

  for (uint32_t frame = 0; frame < m_frameCount; ++frame) {
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;

    bufferInfos.reserve(m_slots.size());
    imageInfos.reserve(totalCapacity);
    writes.reserve(m_slots.size() + images.size());

    for (const Slot& slot : m_slots) {
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = slot.buffers.at(slot.perFrame ? frame : 0).buffer();
      bufferInfo.offset = 0;
      bufferInfo.range = slot.size;

      bufferInfos.push_back(bufferInfo);
    }

    for (size_t i = 0; i < m_slots.size(); ++i) {
      const Slot& slot = m_slots[i];

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_sets[frame];
      write.dstBinding = slot.binding;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write.descriptorCount = 1;
      write.pBufferInfo = bufferInfos.data() + i;

      writes.push_back(write);
    }

    size_t offset = 0;
    for (const ImageBinding& image : images) {
      size_t count = image.elements.size();

      if (count <= 0) {
        continue;
      }

      for (const auto& element : image.elements) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView = element.view;
        imageInfo.sampler = element.sampler;

        imageInfos.push_back(imageInfo);
      }

      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_sets[frame];
      write.dstBinding = image.binding;
      write.dstArrayElement = 0;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.descriptorCount = static_cast<uint32_t>(count);
      write.pImageInfo = imageInfos.data() + offset;

      writes.push_back(write);
      offset += count;
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
