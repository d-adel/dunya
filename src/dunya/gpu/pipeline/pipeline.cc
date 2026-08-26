#include "pipeline.ih"

namespace dunya::gpu {

static bool compileShader(
  const std::string& source,
  const std::string& output
) {
  // Every path is quoted here rather than baked into the flag string, so a
  // directory with a space in it stays one argument.
  const std::string command = "\"\"" GLSLC_PATH "\" " GLSLC_DEFINES
                              " -I\"" GLSLC_INCLUDE_DIR "\" \""
                              + source + "\" -o \"" + output + "\"\"";

  return std::system(command.c_str()) == 0;
}

Pipeline::Pipeline(
  PipelineType type,
  const VkDevice& device,
  const std::vector<VkDescriptorSetLayout>& setLayouts,
  const SwapChain& swapChain,
  std::span<const VkVertexInputBindingDescription> bindingDescriptions,
  std::span<const VkVertexInputAttributeDescription> attributeDescriptions
)
    : m_device(device),
      m_swapChainImageFormat(swapChain.imageFormat()),
      m_setLayouts(setLayouts),
      m_bindingDescriptions(
        bindingDescriptions.begin(),
        bindingDescriptions.end()
      ),
      m_attributeDescriptions(
        attributeDescriptions.begin(),
        attributeDescriptions.end()
      ),
      m_type(type),
      m_depthImageFormat(swapChain.depthImage().format()) {
  try {
    create();
  } catch (...) {
    destroy();
    throw;
  }
}

Pipeline::~Pipeline() {
  destroy();
}

void Pipeline::destroy() noexcept {
  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  m_pipeline = VK_NULL_HANDLE;

  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  m_pipelineLayout = VK_NULL_HANDLE;
}

const VkPipeline& Pipeline::pipeline() const noexcept {
  return m_pipeline;
}

const VkPipelineLayout& Pipeline::pipelineLayout() const noexcept {
  return m_pipelineLayout;
}

void Pipeline::makeConfig() {
  VkPushConstantRange pushConstant{};

  m_config.pushConstantRanges.clear();

  switch (m_type) {
    case PipelineType::Mesh: {
      m_config.vert = SHADER_SOURCE_DIR "/shader.vert";
      m_config.frag = SHADER_SOURCE_DIR "/shader.frag";
      m_config.vertexShader = "shaders/shader.vert.spv";
      m_config.fragmentShader = "shaders/shader.frag.spv";
      m_config.bindingDescriptions = m_bindingDescriptions;
      m_config.attributeDescriptions = m_attributeDescriptions;
      m_config.cullMode = VK_CULL_MODE_BACK_BIT;
      m_config.depthTestEnable = VK_TRUE;
      m_config.depthWriteEnable = VK_TRUE;
      m_config.setLayouts = m_setLayouts;

      pushConstant.offset = 0;
      pushConstant.size = offsetof(PushConstants, objectIndex)
                          + sizeof(PushConstants::objectIndex);
      pushConstant.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      m_config.pushConstantRanges.push_back(pushConstant);
      break;
    }
    case PipelineType::Field:
      m_config.vert = SHADER_SOURCE_DIR "/field-shader.vert";
      m_config.frag = SHADER_SOURCE_DIR "/field-shader.frag";
      m_config.vertexShader = "shaders/field-shader.vert.spv";
      m_config.fragmentShader = "shaders/field-shader.frag.spv";
      m_config.cullMode = VK_CULL_MODE_FRONT_BIT;
      m_config.depthTestEnable = VK_TRUE;
      m_config.depthWriteEnable = VK_TRUE;
      m_config.setLayouts = m_setLayouts;

      pushConstant.offset = 0;
      pushConstant.size = offsetof(PushConstants, objectIndex)
                          + sizeof(PushConstants::objectIndex);
      pushConstant.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      m_config.pushConstantRanges.push_back(pushConstant);
      break;
    default:
      throw std::invalid_argument("Unknown pipeline type");
  }
}

void Pipeline::create() {
  makeConfig();
  m_pipelineLayout = buildPipelineLayout();

  if (m_pipelineLayout == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to create pipeline layout");
  }

  m_pipeline = buildPipeline();
  if (m_pipeline == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to create pipeline");
  }

  m_vertTime = std::filesystem::last_write_time(m_config.vert);
  m_fragTime = std::filesystem::last_write_time(m_config.frag);
  m_includeTime = newestIncludeTime();
}

std::filesystem::file_time_type Pipeline::newestIncludeTime() {
  // A shader's own file is not the whole of its source any more: the shared
  // evaluation lives in an include, and editing that has to reload just the
  // same. Newest wins, so adding a file counts as a change too.
  std::filesystem::file_time_type newest{};
  std::error_code ec;

  for (const auto& entry :
       std::filesystem::directory_iterator(GLSLC_INCLUDE_DIR, ec)) {
    if (ec) {
      break;
    }

    if (entry.path().extension() != ".glsl") {
      continue;
    }

    const auto written = std::filesystem::last_write_time(entry, ec);

    if (!ec && written > newest) {
      newest = written;
    }
  }

  return newest;
}

bool Pipeline::sourcesChanged() const {
  std::error_code ec;
  auto vertTime = std::filesystem::last_write_time(m_config.vert, ec);
  if (ec) {
    return false;
  }
  auto fragTime = std::filesystem::last_write_time(m_config.frag, ec);
  if (ec) {
    return false;
  }

  return vertTime != m_vertTime || fragTime != m_fragTime
         || newestIncludeTime() != m_includeTime;
}

void Pipeline::reload() {
  if (
    compileShader(m_config.vert, m_config.vertexShader) == false
    || compileShader(m_config.frag, m_config.fragmentShader) == false
  ) {
    std::cout << "Shader compilation failed\n";
    return;
  }

  makeConfig();

  // Build first, swap second: the old pipeline stays bound and usable unless a
  // replacement actually exists. buildPipeline reads the .spv from disk, so it
  // throws when a file is missing, and a throw out of here would leave the
  // frame loop with no pipeline and unwind out of main.
  VkPipeline rebuilt = VK_NULL_HANDLE;

  try {
    rebuilt = buildPipeline();
  } catch (const std::exception& e) {
    std::cout << "Pipeline reload failed: " << e.what() << '\n';
    return;
  }

  if (rebuilt == VK_NULL_HANDLE) {
    std::cout << "Pipeline build failed on reload\n";
    return;
  }

  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  m_pipeline = rebuilt;

  m_vertTime = std::filesystem::last_write_time(m_config.vert);
  m_fragTime = std::filesystem::last_write_time(m_config.frag);
  m_includeTime = newestIncludeTime();
}

VkPipelineLayout Pipeline::buildPipelineLayout() {
  VkPipelineLayout pipelineLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(m_config.setLayouts.size());
  pipelineLayoutInfo.pSetLayouts = m_config.setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount =
    static_cast<uint32_t>(m_config.pushConstantRanges.size());
  pipelineLayoutInfo.pPushConstantRanges = m_config.pushConstantRanges.data();

  if (
    vkCreatePipelineLayout(
      m_device,
      &pipelineLayoutInfo,
      nullptr,
      &pipelineLayout
    )
    != VK_SUCCESS
  ) {
    return VK_NULL_HANDLE;
  }

  return pipelineLayout;
}

VkPipeline Pipeline::buildPipeline() {
  ShaderModule vertexShaderModule(m_device, m_config.vertexShader);
  ShaderModule fragShaderModule(m_device, m_config.fragmentShader);

  // Vertex module
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertexShaderModule.handle();
  vertShaderStageInfo.pName = "main";

  // Fragment module
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule.handle();
  fragShaderStageInfo.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
    vertShaderStageInfo,
    fragShaderStageInfo
  };

  std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount =
    static_cast<uint32_t>(m_config.bindingDescriptions.size());
  vertexInputInfo.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(m_config.attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions =
    m_config.bindingDescriptions.data();
  vertexInputInfo.pVertexAttributeDescriptions =
    m_config.attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = m_config.cullMode;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f;           // Optional
  multisampling.pSampleMask = nullptr;             // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE;  // Optional
  multisampling.alphaToOneEnable = VK_FALSE;       // Optional

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;  // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;  // Optional
  colorBlending.blendConstants[1] = 0.0f;  // Optional
  colorBlending.blendConstants[2] = 0.0f;  // Optional
  colorBlending.blendConstants[3] = 0.0f;  // Optional

  VkPipelineRenderingCreateInfo renderingCreateInfo{};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = 1;
  renderingCreateInfo.pColorAttachmentFormats = &m_swapChainImageFormat;
  renderingCreateInfo.depthAttachmentFormat = m_depthImageFormat;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = m_config.depthTestEnable;
  depthStencil.depthWriteEnable = m_config.depthWriteEnable;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f;  // Optional
  depthStencil.maxDepthBounds = 1.0f;  // Optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {};  // Optional
  depthStencil.back = {};   // Optional

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &renderingCreateInfo;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  // Optional
  pipelineInfo.basePipelineIndex = -1;               // Optional
  pipelineInfo.pDepthStencilState = &depthStencil;

  VkPipeline pipeline;
  if (
    vkCreateGraphicsPipelines(
      m_device,
      VK_NULL_HANDLE,
      1,
      &pipelineInfo,
      nullptr,
      &pipeline
    )
    != VK_SUCCESS
  ) {
    return VK_NULL_HANDLE;
  }

  return pipeline;
}

}  // namespace dunya::gpu
