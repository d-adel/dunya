#pragma once

#include "gpu/buffer/buffer.h"
#include "renderer/vertex/vertex.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace dunya::renderer {

class Mesh {
public:
  Mesh(const dunya::gpu::Device& device, std::string modelPath);

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  Mesh(Mesh&&) noexcept = default;
  Mesh& operator=(Mesh&&) noexcept = default;

  size_t indexCount() const noexcept;
  const dunya::gpu::Buffer& vertexBuffer() const noexcept;
  const dunya::gpu::Buffer& indexBuffer() const noexcept;

  ~Mesh() = default;

private:
  void loadModel(const dunya::gpu::Device& device, std::string modelPath);

  dunya::gpu::Buffer m_vertexBuffer;
  dunya::gpu::Buffer m_indexBuffer;
  uint32_t m_indexCount = 0;
};

}  // namespace dunya::renderer
