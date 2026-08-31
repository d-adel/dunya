add_library(dunya_gpu STATIC
    buffer/buffer.cc
    computepipeline/computepipeline.cc
    context/context.cc
    depthimage/depthimage.cc
    descriptorgroup/descriptorgroup.cc
    device/device.cc
    image/image.cc
    instance/instance.cc
    oneshot/oneshotcommand.cc
    pipeline/pipeline.cc
    sampler/sampler.cc
    shadermodule/shadermodule.cc
    surface/surface.cc
    swapchain/swapchain.cc
    texture/texture.cc
    uploader/uploader.cc
)
add_library(dunya::gpu ALIAS dunya_gpu)
target_include_directories(dunya_gpu PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_include_directories(dunya_gpu PRIVATE "${stb_SOURCE_DIR}")

target_compile_features(dunya_gpu PUBLIC cxx_std_23)

target_link_libraries(dunya_gpu PUBLIC
    dunya::stb
    dunya::core
    Vulkan::Vulkan
    glm::glm
)
target_compile_definitions(dunya_gpu PRIVATE
    SHADER_SOURCE_DIR="${SHADER_SOURCE_DIR}"
    GLSLC_PATH="${GLSLC_EXECUTABLE}"
    GLSLC_DEFINES="${SHADER_DEFINES_ARGS}"
    GLSLC_INCLUDE_DIR="${SHADER_SOURCE_DIR}"
)

if (MSVC)
    target_compile_options(dunya_gpu PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_gpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_gpu Runtime)
