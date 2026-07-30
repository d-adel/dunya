#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "field/field.h"

#include <cstdint>
#include <vector>

constexpr uint32_t MAX_PRIMITIVES = 128;

class FieldPass {
public:
  FieldPass(const Device& device, const std::vector<Primitive>& primitives);

  FieldPass(const FieldPass&) = delete;
  FieldPass& operator=(const FieldPass&) = delete;
  FieldPass(FieldPass&&) = delete;
  FieldPass& operator=(FieldPass&&) = delete;

  ~FieldPass() = default;

  void update(uint32_t frame, const FieldFrame& frameData);

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  uint32_t primitiveCount() const noexcept;

private:
  DescriptorGroup m_group;
  uint32_t m_primitiveCount;
};
