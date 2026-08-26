# The hardware layer. It knows Vulkan and the window it draws into, and it
# must not know what a field is - the moment this library includes
# objectmodel or field it has stopped being an RHI and become a renderer.
# Unreal draws the same line between RHI and Renderer.

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
)

add_library(dunya::gpu ALIAS dunya_gpu)

target_include_directories(dunya_gpu PUBLIC "${PROJECT_SOURCE_DIR}/src")

# PRIVATE, and the same reasoning imagecompare gives: stb appears only in
# texture.ih, never in a header a consumer includes.
target_include_directories(dunya_gpu PRIVATE "${stb_SOURCE_DIR}")

target_compile_features(dunya_gpu PUBLIC cxx_std_20)

# PUBLIC: Vulkan handles, the window and glm types all appear in headers
# consumers include. imagecompare is where stb's implementations live, which
# is how texture loading avoids a second copy that would collide at link time.
target_link_libraries(dunya_gpu PUBLIC
    dunya::core
    dunya::platform
    Vulkan::Vulkan
    glm::glm
    dunya::imagecompare
)

# PUBLIC: it changes what <GLFW/glfw3.h> declares, and surface.h is a header
# consumers include - without it Surface has no VkSurfaceKHR to return. It
# lives here rather than on platform because pulling in vulkan.h needs the
# Vulkan include path, and this is the layer that links it.
target_compile_definitions(dunya_gpu PUBLIC GLFW_INCLUDE_VULKAN)

target_compile_definitions(dunya_gpu PRIVATE
    SHADER_SOURCE_DIR="${SHADER_SOURCE_DIR}"
    GLSLC_PATH="${GLSLC_EXECUTABLE}"
    GLSLC_DEFINES="${SHADER_DEFINES_ARGS}"
    GLSLC_INCLUDE_DIR="${SHADER_SOURCE_DIR}"
)

if (MSVC)
    target_compile_options(dunya_gpu PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_gpu PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
