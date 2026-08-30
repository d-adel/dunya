#pragma once

#include <dunya/assets/assetlibrary/assetlibrary.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/gpu/swapchain/swapchain.h>
#include <dunya/gpu/uploader/uploader.h>
#include <dunya/gpu/windowsystem/windowsystem.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/framepacker/framepacker.h>
#include <dunya/renderer/renderer.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/sdfbaker/sdfbaker.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/sdfresidency/sdfresidency.h>
#include <dunya/renderer/volumepool/volumepool.h>

#include <filesystem>
#include <memory>
#include <string>

namespace dunya::capi {

class Session {
public:
  Session(
    std::unique_ptr<dunya::gpu::WindowSystem> windowSystem,
    const std::filesystem::path& projectRoot,
    const std::string& world
  );
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  void resize();

  void retarget(std::unique_ptr<dunya::gpu::WindowSystem> windowSystem);

  void render();

  [[nodiscard]] VkExtent2D extent() const noexcept;

  [[nodiscard]] const dunya::objectmodel::World& world() const noexcept;

private:
  void loadWorld(
    const std::filesystem::path& projectRoot,
    const std::string& world
  );

  void lookAtWorld(float aspect);

  std::unique_ptr<dunya::gpu::WindowSystem> m_windowSystem;
  dunya::gpu::Context m_context;
  dunya::gpu::SwapChain m_swapChain;

  dunya::assets::AssetLibrary m_assetLibrary;
  dunya::objectmodel::World m_world;

  dunya::gpu::Uploader m_uploader;
  dunya::renderer::FrameGlobals m_frameGlobals;
  dunya::renderer::ResourceTable m_resourceTable;
  dunya::renderer::SdfRecordTable m_recordTable;
  dunya::renderer::SdfBaker m_sdfBaker;
  dunya::renderer::VolumePool m_volumePool;
  dunya::renderer::SdfResidency m_residency;
  dunya::renderer::FramePacker m_framePacker;

  dunya::gpu::Pipeline m_meshPipeline;
  dunya::gpu::Pipeline m_sdfPipeline;
  dunya::renderer::Renderer m_renderer;

  dunya::renderer::Frame m_frame;
};

}
