#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/renderer/framepacker/framepacker.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/sdfbaker/sdfbaker.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/sdfresidency/sdfresidency.h>
#include <dunya/renderer/volumepool/volumepool.h>

#include <span>

namespace dunya::renderer {

class RendererStorage {
public:
  RendererStorage(
    const dunya::gpu::Device& device,
    std::span<const dunya::gpu::Texture> textures,
    std::span<const dunya::gpu::Sampler> samplers,
    std::span<const MaterialRecord> materials
  );

  RendererStorage(const RendererStorage&) = delete;
  RendererStorage& operator=(const RendererStorage&) = delete;
  RendererStorage(RendererStorage&&) = delete;
  RendererStorage& operator=(RendererStorage&&) = delete;

  ~RendererStorage() = default;

  [[nodiscard]] dunya::gpu::Uploader& uploader() noexcept;

  [[nodiscard]] FrameGlobals& frameGlobals() noexcept;

  [[nodiscard]] const FrameGlobals& frameGlobals() const noexcept;

  [[nodiscard]] ResourceTable& resourceTable() noexcept;

  [[nodiscard]] SdfRecordTable& recordTable() noexcept;

  [[nodiscard]] const SdfRecordTable& recordTable() const noexcept;

  [[nodiscard]] const SdfBaker& sdfBaker() const noexcept;

  [[nodiscard]] VolumePool& volumePool() noexcept;

  [[nodiscard]] const VolumePool& volumePool() const noexcept;

  [[nodiscard]] SdfResidency& residency() noexcept;

  [[nodiscard]] FramePacker& framePacker() noexcept;

private:
  dunya::gpu::Uploader m_uploader;
  FrameGlobals m_frameGlobals;
  ResourceTable m_resourceTable;
  SdfRecordTable m_recordTable;
  SdfBaker m_sdfBaker;
  VolumePool m_volumePool;
  SdfResidency m_residency;
  FramePacker m_framePacker;
};

}
