#pragma once

#include <vulkan/vulkan.h>

#include <functional>

namespace dunya::renderer {

enum class PassOrder {
  BeforeScene,
  AfterScene
};

struct ScenePass {
  PassOrder order = PassOrder::AfterScene;

  std::function<void(VkCommandBuffer)> draw;
};

}
