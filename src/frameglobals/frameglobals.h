#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"

#include <glm/glm.hpp>

#include <cstdint>

struct CameraUniform {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewProj;
  glm::mat4 inverseViewProj;
  glm::vec4 position;
};

class FrameGlobals {
public:
  FrameGlobals(const Device& device);

  FrameGlobals(FrameGlobals const&) = delete;
  FrameGlobals& operator=(FrameGlobals const&) = delete;
  FrameGlobals(FrameGlobals&&) = delete;
  FrameGlobals& operator=(FrameGlobals&&) = delete;

  ~FrameGlobals() = default;

  void update(uint32_t frame, const CameraUniform& camera);

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  DescriptorGroup m_group;
};
