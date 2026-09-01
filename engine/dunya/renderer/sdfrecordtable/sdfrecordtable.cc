#include "sdfrecordtable.ih"

namespace dunya::renderer {

namespace {

constexpr VkDeviceSize BRICK_BOUNDS_BYTES =
  static_cast<VkDeviceSize>(dunya::core::MAX_SDF_VOLUMES)
  * dunya::core::BRICK_TABLE_STRIDE * sizeof(float);

}

SdfRecordTable::SdfRecordTable(const dunya::gpu::Device& device)
    : m_device(device),
      m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{ENTRIES,
          dunya::core::MAX_SDF_RECORDS * sizeof(SdfRecord),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_COMPUTE_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {PRIMITIVE_POOL,
          dunya::core::MAX_PRIMITIVE_POOL * sizeof(dunya::field::Primitive),
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {RECORD_BOUNDS,
          dunya::core::MAX_SDF_RECORDS * sizeof(RecordBounds),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {SHADOW_CELLS,
          SHADOW_GRID_CELLS * SHADOW_GRID_CELLS * sizeof(ShadowCell),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {SHADOW_INDICES,
          SHADOW_GRID_MAX_INDICES * sizeof(uint32_t),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {SHADOW_GRID,
          sizeof(ShadowGridUniform),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame}},
        {{DISTANCE_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          dunya::core::MAX_SDF_VOLUMES},
         {MATERIAL_VOLUMES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          {},
          dunya::core::MAX_SDF_VOLUMES}},
        {},
        {{DISTANCE_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_SDF_VOLUMES},
         {MATERIAL_VOLUMES_STORAGE,
          VK_SHADER_STAGE_COMPUTE_BIT,
          {},
          dunya::core::MAX_SDF_VOLUMES}},
        {{BRICK_BOUNDS,
          VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT}}
      ),
      m_brickBounds(
        device,
        BRICK_BOUNDS_BYTES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
          | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      ) {
  m_records.resize(dunya::core::MAX_SDF_RECORDS);
  m_bakeQueued.resize(dunya::core::MAX_SDF_VOLUMES, 0u);
  m_recordBounds.resize(dunya::core::MAX_SDF_RECORDS);

  m_group.writeBuffer(BRICK_BOUNDS, m_brickBounds.buffer(), BRICK_BOUNDS_BYTES);
}

void SdfRecordTable::registerVolume(
  VkImageView distanceView,
  VkImageView materialView,
  uint32_t volumeIndex
) {
  m_group.writeImage(DISTANCE_VOLUMES, volumeIndex, distanceView);
  m_group.writeImage(MATERIAL_VOLUMES, volumeIndex, materialView);
  m_group.writeStorageImage(
    DISTANCE_VOLUMES_STORAGE,
    volumeIndex,
    distanceView
  );
  m_group.writeStorageImage(
    MATERIAL_VOLUMES_STORAGE,
    volumeIndex,
    materialView
  );
}

void SdfRecordTable::uploadBounds(
  dunya::gpu::Uploader& uploader,
  uint32_t volumeIndex,
  const dunya::field::SampledSdf& field
) {
  VkDeviceSize sizeBytes = 0;
  dunya::gpu::Buffer staging = stageBounds(field, sizeBytes);

  recordBounds(uploader.begin(), staging, volumeIndex, sizeBytes);

  uploader.keep(std::move(staging));
}

void SdfRecordTable::uploadBounds(
  uint32_t volumeIndex,
  const dunya::field::SampledSdf& field
) {
  VkDeviceSize sizeBytes = 0;
  const dunya::gpu::Buffer staging = stageBounds(field, sizeBytes);

  dunya::gpu::OneShotCommand cmd;
  cmd.start(m_device);

  recordBounds(cmd.cmdBuffer(), staging, volumeIndex, sizeBytes);

  cmd.submit(m_device);
}

dunya::gpu::Buffer SdfRecordTable::stageBounds(
  const dunya::field::SampledSdf& field,
  VkDeviceSize& sizeBytes
) const {
  std::vector<float> table;
  table.reserve(1u + field.brickLipschitz.size());
  table.push_back(field.globalLipschitz);
  table.insert(
    table.end(),
    field.brickLipschitz.begin(),
    field.brickLipschitz.end()
  );

  if (table.size() > dunya::core::BRICK_TABLE_STRIDE) {
    throw std::runtime_error("A field has more bricks than its table slot");
  }

  sizeBytes = static_cast<VkDeviceSize>(table.size()) * sizeof(float);

  dunya::gpu::Buffer staging(
    m_device,
    sizeBytes,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  void* mapped = nullptr;

  if (
    vkMapMemory(m_device.vkDevice(), staging.memory(), 0, sizeBytes, 0, &mapped)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to map brick bound staging memory");
  }

  memcpy(mapped, table.data(), static_cast<size_t>(sizeBytes));
  vkUnmapMemory(m_device.vkDevice(), staging.memory());

  return staging;
}

void SdfRecordTable::recordBounds(
  VkCommandBuffer commandBuffer,
  const dunya::gpu::Buffer& staging,
  uint32_t volumeIndex,
  VkDeviceSize sizeBytes
) const {
  VkMemoryBarrier2 boundsFree{};
  boundsFree.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  boundsFree.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                            | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  boundsFree.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
  boundsFree.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  boundsFree.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

  VkDependencyInfo freeDependency{};
  freeDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  freeDependency.memoryBarrierCount = 1;
  freeDependency.pMemoryBarriers = &boundsFree;

  vkCmdPipelineBarrier2(commandBuffer, &freeDependency);

  VkBufferCopy region{};
  region.srcOffset = 0;
  region.dstOffset = static_cast<VkDeviceSize>(volumeIndex)
                     * dunya::core::BRICK_TABLE_STRIDE * sizeof(float);
  region.size = sizeBytes;

  vkCmdCopyBuffer(
    commandBuffer,
    staging.buffer(),
    m_brickBounds.buffer(),
    1,
    &region
  );

  VkMemoryBarrier2 boundsVisible{};
  boundsVisible.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  boundsVisible.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  boundsVisible.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  boundsVisible.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                               | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  boundsVisible.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

  VkDependencyInfo visibleDependency{};
  visibleDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  visibleDependency.memoryBarrierCount = 1;
  visibleDependency.pMemoryBarriers = &boundsVisible;

  vkCmdPipelineBarrier2(commandBuffer, &visibleDependency);
}

const dunya::gpu::Buffer& SdfRecordTable::brickBounds() const noexcept {
  return m_brickBounds;
}

void SdfRecordTable::newFrame() {
  for (const uint32_t slot : m_bakeDispatch) {
    m_bakeQueued[m_records[slot].resolutionVolumeIndex.w] = 0u;
  }

  m_bakeList.clear();
  m_bakeDispatch.clear();
}

void SdfRecordTable::appendToBakeList(uint32_t slot) {
  m_bakeList.push_back(slot);

  const uint32_t volume = m_records.at(slot).resolutionVolumeIndex.w;

  if (volume >= m_bakeQueued.size() || m_bakeQueued[volume] != 0u) {
    return;
  }

  m_bakeQueued[volume] = 1u;
  m_bakeDispatch.push_back(slot);
}

void SdfRecordTable::setRecord(
  uint32_t recordIndex,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::SdfGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t fieldRepresentation,
  std::span<const dunya::field::Primitive> primitives
) {
  m_records[recordIndex] = makeSdfRecord(
    pose,
    grid,
    volume,
    primitiveOffset,
    primitiveCount,
    fieldRepresentation,
    dunya::objectmodel::gridMargin(grid, primitives)
  );

  m_recordBounds[recordIndex] = makeRecordBounds(
    dunya::objectmodel::casterBox(primitives, grid),
    m_records[recordIndex].model
  );
}

void SdfRecordTable::update(
  uint32_t frame,
  uint32_t liveRecords,
  const glm::vec3& toLight
) {
  std::span<const SdfRecord> objects(m_records);

  m_group.write(ENTRIES, frame, objects.data(), objects.size_bytes());

  std::span<const RecordBounds> bounds(m_recordBounds);

  m_group.write(RECORD_BOUNDS, frame, bounds.data(), bounds.size_bytes());

  m_shadowGrid.build(bounds, liveRecords, toLight);

  const std::span<const ShadowCell> cells = m_shadowGrid.cells();
  const std::span<const uint32_t> indices = m_shadowGrid.indices();

  if (!cells.empty()) {
    m_group.write(SHADOW_CELLS, frame, cells.data(), cells.size_bytes());
  }

  if (!indices.empty()) {
    m_group.write(SHADOW_INDICES, frame, indices.data(), indices.size_bytes());
  }

  const ShadowGridUniform& grid = m_shadowGrid.uniform();

  m_group.write(SHADOW_GRID, frame, &grid, sizeof(grid));
}

void SdfRecordTable::updatePrimitives(
  uint32_t frame,
  std::span<const dunya::field::Primitive> primitives
) {
  m_group.write(
    PRIMITIVE_POOL,
    frame,
    primitives.data(),
    primitives.size_bytes()
  );
}

const VkDescriptorSet& SdfRecordTable::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& SdfRecordTable::setLayout() const noexcept {
  return m_group.setLayout();
}

std::span<const SdfRecord> SdfRecordTable::records() const noexcept {
  return m_records;
}

std::span<const uint32_t> SdfRecordTable::bakeList() const noexcept {
  return m_bakeList;
}

std::span<const uint32_t> SdfRecordTable::bakeDispatch() const noexcept {
  return m_bakeDispatch;
}

const SdfRecord& SdfRecordTable::record(uint32_t recordIndex) const {
  return m_records.at(recordIndex);
}

}
