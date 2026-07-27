#pragma once

#include "buffer/buffer.h"
#include "vertex/vertex.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <string>

class Mesh {
public:
  Mesh(const Device& device, std::string modelPath);

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  Mesh(Mesh&&) noexcept = default;
  Mesh& operator=(Mesh&&) noexcept = default;

  size_t indexCount() const noexcept;
  const Buffer& vertexBuffer() const noexcept;
  const Buffer& indexBuffer() const noexcept;

  ~Mesh() = default;

private:
  void loadModel(const Device& device, std::string modelPath);

  Buffer m_vertexBuffer;
  Buffer m_indexBuffer;
  uint32_t m_indexCount = 0;
};
