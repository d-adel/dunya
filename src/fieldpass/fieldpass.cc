#include "fieldpass.ih"

using dunya::field::Primitive;
using dunya::field::SampledField;

namespace {

// The box the grid covers: what the bounded primitives span, plus the slack
// that keeps their surfaces off the boundary. The margin is load-bearing - the
// shader's bound for a point outside the grid leans on nothing inside it
// reaching the edge.
dunya::field::Aabb paddedExtent(std::span<const Primitive> primitives) {
  const std::optional<dunya::field::Aabb> extent =
    dunya::field::boundedExtent(primitives);

  // With nothing bounded there is nothing a grid could hold, so cover the
  // smallest legal box rather than special-casing every use of it.
  const dunya::field::Aabb box =
    extent.value_or(dunya::field::Aabb{glm::vec3(0.0f), glm::vec3(1.0f)});

  const glm::vec3 margin(FIELD_GRID_MARGIN);

  return {box.minimum - margin, box.maximum + margin};
}

SampledField bakeGrid(
  const dunya::field::Aabb& box,
  std::span<const Primitive> primitives
) {
  const auto start = std::chrono::steady_clock::now();

  SampledField grid = dunya::field::bake(
    primitives,
    box.minimum,
    box.maximum,
    glm::uvec3(FIELD_GRID_RESOLUTION)
  );

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start
  );

  const size_t texels = grid.distances.size();

  std::cout << "field grid " << FIELD_GRID_RESOLUTION << "^3  baked in "
            << elapsed.count() << " ms  distance "
            << (texels * sizeof(float)) / (1024 * 1024) << " MB  material "
            << (texels) / (1024 * 1024) << " MB\n";

  return grid;
}

Texture makeDistanceVolume(const Device& device, const SampledField& grid) {
  // 32-bit for the first measurement, deliberately: half's spacing near a
  // distance of one is about 0.001, which is the march epsilon, so starting
  // narrower would fold quantisation error into the interpolation number this
  // milestone is trying to measure. R16 and R8 are the next data points.
  return Texture(
    device,
    grid.resolution.x,
    grid.resolution.y,
    grid.resolution.z,
    VK_FORMAT_R32_SFLOAT,
    grid.distances.data(),
    grid.distances.size() * sizeof(float),
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
  );
}

Texture makeMaterialVolume(const Device& device, const SampledField& grid) {
  std::vector<uint8_t> ids;
  ids.reserve(grid.materials.size());

  for (uint32_t material : grid.materials) {
    ids.push_back(static_cast<uint8_t>(material));
  }

  return Texture(
    device,
    grid.resolution.x,
    grid.resolution.y,
    grid.resolution.z,
    VK_FORMAT_R8_UINT,
    ids.data(),
    ids.size(),
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
  );
}

}  // namespace

FieldPass::FieldPass(
  const Device& device,
  const FieldObjectTable& table,
  const dunya::field::Aabb& box,
  std::span<const dunya::field::Primitive> primitives
)
    : m_device(device),
      m_grid(bakeGrid(box, primitives)),
      m_distanceVolume(makeDistanceVolume(device, m_grid)),
      m_materialVolume(makeMaterialVolume(device, m_grid)),
      m_table(table),
      m_bakePipeline(
        device.vkDevice(),
        "shaders/field-bake.comp.spv",
        std::vector<VkDescriptorSetLayout>{table.setLayout()},
        std::vector<VkPushConstantRange>{
          {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BakeParams)}
        }
      ) {
  if (primitives.size() > MAX_PRIMITIVES) {
    throw std::runtime_error("More primitives than the field buffer holds");
  }

  primitivesChanged(primitives);
}

namespace {

VkImageMemoryBarrier2 volumeBarrier(
  VkImage image,
  VkImageLayout from,
  VkImageLayout to
) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask =
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  return barrier;
}

void transitionVolumes(
  VkCommandBuffer cmd,
  VkImage distances,
  VkImage materials,
  VkImageLayout from,
  VkImageLayout to
) {
  const std::array<VkImageMemoryBarrier2, 2> barriers{
    volumeBarrier(distances, from, to),
    volumeBarrier(materials, from, to)
  };

  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
  dependency.pImageMemoryBarriers = barriers.data();

  vkCmdPipelineBarrier2(cmd, &dependency);
}

}  // namespace

void FieldPass::bakeIfDirty(
  uint32_t frame,
  uint32_t primitiveCount,
  uint32_t volume
) {
  if (!m_gridDirty) {
    return;
  }

  m_gridDirty = false;

  const auto start = std::chrono::steady_clock::now();

  OneShotCommand cmd;
  cmd.start(m_device);

  // A storage image is written in GENERAL; the fragment pass samples it in a
  // read-only layout, so the dispatch is bracketed by the two transitions.
  transitionVolumes(
    cmd.cmdBuffer(),
    m_distanceVolume.image().image(),
    m_materialVolume.image().image(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_GENERAL
  );

  vkCmdBindPipeline(
    cmd.cmdBuffer(),
    VK_PIPELINE_BIND_POINT_COMPUTE,
    m_bakePipeline.pipeline()
  );

  vkCmdBindDescriptorSets(
    cmd.cmdBuffer(),
    VK_PIPELINE_BIND_POINT_COMPUTE,
    m_bakePipeline.pipelineLayout(),
    0,
    1,
    &m_table.descriptorSet(frame),
    0,
    nullptr
  );

  const BakeParams params{
    glm::vec4(m_grid.origin, 0.0f),
    glm::vec4(m_grid.voxelSize, 0.0f),
    glm::uvec4(m_grid.resolution, primitiveCount),
    glm::uvec4(volume, 0u, 0u, 0u)
  };

  vkCmdPushConstants(
    cmd.cmdBuffer(),
    m_bakePipeline.pipelineLayout(),
    VK_SHADER_STAGE_COMPUTE_BIT,
    0,
    sizeof(params),
    &params
  );

  // Rounded up, with the shader discarding the tail past the lattice.
  const uint32_t groups = (FIELD_GRID_RESOLUTION + 3u) / 4u;
  vkCmdDispatch(cmd.cmdBuffer(), groups, groups, groups);

  transitionVolumes(
    cmd.cmdBuffer(),
    m_distanceVolume.image().image(),
    m_materialVolume.image().image(),
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  );

  cmd.submit(m_device);

  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - start
  );

  std::cout << "field grid rebaked on GPU in " << elapsed.count() << " us  ("
            << primitiveCount << " primitives)\n";
}

namespace {

// Pulls a whole volume back into host memory. Only ever called by the bake
// check, so it takes the simple route and waits for the copy.
std::vector<uint8_t> readVolume(
  const Device& device,
  const Image& image,
  const glm::uvec3& resolution,
  VkDeviceSize sizeBytes
) {
  Buffer readback(
    device,
    sizeBytes,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  OneShotCommand cmd;
  cmd.start(device);

  const std::array<VkImageMemoryBarrier2, 1> toSource{volumeBarrier(
    image.image(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  )};

  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = toSource.data();

  vkCmdPipelineBarrier2(cmd.cmdBuffer(), &dependency);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {resolution.x, resolution.y, resolution.z};

  vkCmdCopyImageToBuffer(
    cmd.cmdBuffer(),
    image.image(),
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    readback.buffer(),
    1,
    &region
  );

  const std::array<VkImageMemoryBarrier2, 1> toRead{volumeBarrier(
    image.image(),
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  )};

  dependency.pImageMemoryBarriers = toRead.data();
  vkCmdPipelineBarrier2(cmd.cmdBuffer(), &dependency);

  cmd.submit(device);

  std::vector<uint8_t> bytes(static_cast<size_t>(sizeBytes));

  void* mapped = nullptr;
  if (
    vkMapMemory(device.vkDevice(), readback.memory(), 0, sizeBytes, 0, &mapped)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to map the bake readback buffer");
  }

  std::memcpy(bytes.data(), mapped, static_cast<size_t>(sizeBytes));
  vkUnmapMemory(device.vkDevice(), readback.memory());

  return bytes;
}

}  // namespace

/* Checks the compute bake against the CPU one it is supposed to reproduce.
 *
 * The two are separate implementations of the same fold, in different
 * languages, and M17's whole comparison rests on them agreeing. Nothing else
 * tests that: the renderer only ever displays the GPU result, so a divergence
 * would look like the grid's interpolation error rather than like a bug.
 */
void FieldPass::verifyBake(
  std::span<const dunya::field::Primitive> primitives
) {
  const dunya::field::Aabb box = paddedExtent(primitives);

  const SampledField reference = dunya::field::bake(
    primitives,
    box.minimum,
    box.maximum,
    glm::uvec3(FIELD_GRID_RESOLUTION)
  );

  const std::vector<uint8_t> distanceBytes = readVolume(
    m_device,
    m_distanceVolume.image(),
    reference.resolution,
    reference.distances.size() * sizeof(float)
  );

  const std::vector<uint8_t> materialBytes = readVolume(
    m_device,
    m_materialVolume.image(),
    reference.resolution,
    reference.materials.size()
  );

  const float* baked = reinterpret_cast<const float*>(distanceBytes.data());

  float worstDistance = 0.0f;
  size_t materialMismatches = 0;

  for (size_t i = 0; i < reference.distances.size(); ++i) {
    worstDistance =
      std::max(worstDistance, std::abs(baked[i] - reference.distances[i]));

    if (materialBytes[i] != static_cast<uint8_t>(reference.materials[i])) {
      ++materialMismatches;
    }
  }

  std::cout << "bake check  worst distance " << std::scientific << worstDistance
            << "  material mismatches " << materialMismatches << " of "
            << reference.distances.size() << '\n';
}

void FieldPass::primitivesChanged(
  std::span<const dunya::field::Primitive> primitives
) {
  if (primitives.size() > MAX_PRIMITIVES) {
    throw std::runtime_error("More primitives than the field buffer holds");
  }

  m_gridDirty = true;

  // The grid follows what it holds. An edit that grows or moves the bounded
  // primitives moves the box with them, or the new geometry lands outside the
  // lattice and the sampled representation simply does not have it. The
  // resolution never changes, so the volumes keep their storage.
  const dunya::field::Aabb box = paddedExtent(primitives);

  m_grid.origin = box.minimum;
  m_grid.voxelSize =
    dunya::field::voxelSize(box.minimum, box.maximum, m_grid.resolution);
}

VkImageView FieldPass::distanceVolume() const noexcept {
  return m_distanceVolume.image().imageView();
}

VkImageView FieldPass::materialVolume() const noexcept {
  return m_materialVolume.image().imageView();
}
