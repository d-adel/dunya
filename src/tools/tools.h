#pragma once

#include <dunya/field/raycast/raycast.h>
#include <dunya/objectmodel/world/world.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace dunya::gpu {

class Context;
class SwapChain;

}

class Application;

class Tools {
public:
  virtual ~Tools() = default;

  virtual bool wantsMouse() const = 0;
  virtual bool wantsKeyboard() const = 0;

  virtual void notice(std::string text) = 0;

  virtual void build(Application& application) = 0;
  virtual void record(VkCommandBuffer commandBuffer) const = 0;

  virtual void edit(uint32_t operation, const dunya::field::Ray& ray) = 0;
  virtual void stress(uint32_t count) = 0;
  virtual void undo() = 0;
  virtual void redo() = 0;
  virtual void retarget(dunya::objectmodel::World& world) = 0;
};

using ToolsFactory = std::function<std::unique_ptr<Tools>(
  const dunya::gpu::Context&,
  const dunya::gpu::SwapChain&,
  dunya::objectmodel::World&
)>;
