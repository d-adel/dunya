cmake_minimum_required(VERSION 3.28)

project(DunyaRenderer VERSION 0.0 LANGUAGES CXX)

# Demand C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)


# Fetch glfw
include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.4
)

set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_EXAMPLES OFF)

# Shaders
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# Textures
set(TEXTURE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/textures")

# Models
set(MODEL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")

# Fetch glm
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm
  GIT_TAG        0af55ccecd98d4e5a8d1fad7de25ba429d60e863
)

# Fetch stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY  https://github.com/nothings/stb
    GIT_TAG         31c1ad37456438565541f4919958214b6e762fb4
)

# Fetch tinyobjloader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY  https://github.com/tinyobjloader/tinyobjloader
    GIT_TAG         e60d33385e2e4f7fa891513150f2532b5bbcb093
    SOURCE_SUBDIR  cmake_disabled
)

FetchContent_MakeAvailable(glfw glm stb tinyobjloader)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Define executable and link libraries
add_executable(${PROJECT_NAME}
    src/main.cc
    src/instance/instance.cc
    src/window/window.cc
    src/device/device.cc
    src/surface/surface.cc
    src/swapchain/swapchain.cc
    src/pipeline/pipeline.cc
    src/shadermodule/shadermodule.cc
    src/renderer/renderer.cc
    src/buffer/buffer.cc
    src/vertex/vertex.cc
    src/frameglobals/frameglobals.cc
    src/resourcetable/resourcetable.cc
    src/descriptorgroup/descriptorgroup.cc
    src/image/image.cc
    src/oneshot/oneshotcommand.cc
    src/texture/stb_image_impl.cc
    src/texture/texture.cc
    src/sampler/sampler.cc
    src/depthimage/depthimage.cc
    src/mesh/tiny_obj_loader_impl.cc
    src/mesh/mesh.cc
    src/camera/camera.cc
    src/event/event.cc
    src/input/input.cc
    src/application/application.cc
    src/glfwlibrary/glfwlibrary.cc
    src/context/context.cc
    src/scene/scene.cc
    src/frame/frame.cc
    src/field/field.cc
    src/fieldpass/fieldpass.cc
)

find_program(
    GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)

# Configuration
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    SHADER_SOURCE_DIR=\"${SHADER_SOURCE_DIR}\"
    GLSLC_PATH=\"${GLSLC_EXECUTABLE}\"
)

file(
    GLOB SHADER_SOURCES
    CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.comp"
    "${SHADER_SOURCE_DIR}/*.geom"
    "${SHADER_SOURCE_DIR}/*.tesc"
    "${SHADER_SOURCE_DIR}/*.tese"
)

set(SHADER_OUTPUTS)

foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_FILENAME "${SHADER_SOURCE}" NAME)

    set(SHADER_OUTPUT
        "${SHADER_OUTPUT_DIR}/${SHADER_FILENAME}.spv"
    )

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"

        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "${SHADER_OUTPUT_DIR}"

        COMMAND
            "${GLSLC_EXECUTABLE}"
            "${SHADER_SOURCE}"
            -o "${SHADER_OUTPUT}"

        DEPENDS "${SHADER_SOURCE}"

        COMMENT "Compiling shader ${SHADER_FILENAME}"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()

add_custom_target(
    SHADER_TARGET
    DEPENDS ${SHADER_OUTPUTS}
)

add_dependencies(${PROJECT_NAME} SHADER_TARGET)

# Target
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan glm::glm)
target_include_directories(${PROJECT_NAME} PRIVATE src ${stb_SOURCE_DIR} ${tinyobjloader_SOURCE_DIR})

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Copying shaders"
    VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${TEXTURE_SOURCE_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/textures
    COMMENT "Copying textures"
    VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${MODEL_SOURCE_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
    COMMENT "Copying models"
    VERBATIM
)
