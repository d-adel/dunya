#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "field/field.h"

#include <cstdint>
#include <span>

struct FieldFrame {
  glm::uvec4 primitiveCount;
};

class FieldPass {
public:
  FieldPass(
    const Device& device,
    std::span<const dunya::field::Primitive> primitives
  );

  FieldPass(const FieldPass&) = delete;
  FieldPass& operator=(const FieldPass&) = delete;
  FieldPass(FieldPass&&) = delete;
  FieldPass& operator=(FieldPass&&) = delete;

  ~FieldPass() = default;

  void update(uint32_t frame, const FieldFrame& frameData);

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;

private:
  DescriptorGroup m_group;
};
