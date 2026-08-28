#pragma once

#include <dunya/gpu/buffer/buffer.h>
#include <dunya/renderer/vertex/vertex.h>

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace dunya::renderer {

class MeshBuffers {
public:
  MeshBuffers(const dunya::gpu::Device& device, std::string modelPath);

  MeshBuffers(const MeshBuffers&) = delete;
  MeshBuffers& operator=(const MeshBuffers&) = delete;

  MeshBuffers(MeshBuffers&&) noexcept = default;
  MeshBuffers& operator=(MeshBuffers&&) noexcept = default;

  size_t indexCount() const noexcept;
  const dunya::gpu::Buffer& vertexBuffer() const noexcept;
  const dunya::gpu::Buffer& indexBuffer() const noexcept;

  ~MeshBuffers() = default;

private:
  void loadModel(const dunya::gpu::Device& device, std::string modelPath);

  dunya::gpu::Buffer m_vertexBuffer;
  dunya::gpu::Buffer m_indexBuffer;
  uint32_t m_indexCount = 0;
};

}  // namespace dunya::renderer
