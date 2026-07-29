#include "fieldpass.ih"

FieldPass::FieldPass(
  const Device& device,
  const std::vector<Primitive>& primitives
)
    : m_device(device.vkDevice()), m_primitives(primitives) {
  createSetLayout();
  createUniformBuffer(device);
  createDescriptorPool();
  createDescriptorSet();
}

FieldPass::~FieldPass() {
  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
}

const VkDescriptorSetLayout& FieldPass::setLayout() const noexcept {
  return m_setLayout;
}

const VkDescriptorSet& FieldPass::descriptorSet() const noexcept {
  return m_descriptorSet;
}

const std::vector<Primitive>& FieldPass::primitives() const noexcept {
  return m_primitives;
}

void FieldPass::createSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  uboLayoutBinding.pImmutableSamplers = nullptr;  // Optional

  std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
    uboLayoutBinding,
  };

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

void FieldPass::createUniformBuffer(const Device& device) {
  VkDeviceSize bufferSize = MAX_PRIMITIVES * sizeof(Primitive);

  m_uniformBuffer = Buffer(
    device,
    bufferSize,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

  );

  vkMapMemory(
    device.vkDevice(),
    m_uniformBuffer.memory(),
    0,
    bufferSize,
    0,
    &m_uniformBufferMapped
  );

  memcpy(
    m_uniformBufferMapped,
    m_primitives.data(),
    m_primitives.size() * sizeof(Primitive)
  );
}

void FieldPass::createDescriptorPool() {
  std::array<VkDescriptorPoolSize, 1> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1;

  if (
    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create descriptor pool");
  }
}

void FieldPass::createDescriptorSet() {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_setLayout;

  if (
    vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to allocate descriptor sets");
  }

  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_uniformBuffer.buffer();
  bufferInfo.offset = 0;
  bufferInfo.range = MAX_PRIMITIVES * sizeof(Primitive);

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = m_descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;

  vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}
