cmake_minimum_required(VERSION 3.28)

project(Dunya VERSION 0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/O2 /Ob2")
    add_compile_options(/Zc:preprocessor)
endif()
include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(DUNYA_MAX_FIELD_PRIMITIVES 128)
set(DUNYA_MAX_TEXTURES 16)
set(DUNYA_MAX_SAMPLERS 8)
set(DUNYA_MAX_MATERIALS 64)
set(DUNYA_MAX_FIELD_RECORDS 1280)
set(DUNYA_MAX_FIELD_VOLUMES 1280)

set(DUNYA_MAX_PRIMITIVE_POOL 8192)
set(DUNYA_BRICK_CELLS 8)

set(DUNYA_MARCH_EPSILON 0.001f)
set(DUNYA_MARCH_MAX_ITERATIONS 800)
set(DUNYA_SHADOW_MAX_DISTANCE 20.0f)
set(DUNYA_MARCH_MAX_DISTANCE 100.0f)
set(DUNYA_GRADIENT_EPSILON 0.01f)
set(DUNYA_MARCH_OMEGA 1.2f)
set(DUNYA_SHADOW_SHARPNESS 12.0f)
set(DUNYA_SAMPLER_LINEAR_REPEAT 0)
set(DUNYA_SAMPLER_LINEAR_CLAMP 1)
set(DUNYA_SAMPLER_NEAREST_CLAMP 2)
set(SHADER_DEFINES
    "-DDUNYA_MAX_FIELD_PRIMITIVES=${DUNYA_MAX_FIELD_PRIMITIVES}"
    "-DDUNYA_MAX_TEXTURES=${DUNYA_MAX_TEXTURES}"
    "-DDUNYA_MAX_SAMPLERS=${DUNYA_MAX_SAMPLERS}"
    "-DDUNYA_MAX_MATERIALS=${DUNYA_MAX_MATERIALS}"
    "-DDUNYA_MAX_FIELD_RECORDS=${DUNYA_MAX_FIELD_RECORDS}"
    "-DDUNYA_MAX_FIELD_VOLUMES=${DUNYA_MAX_FIELD_VOLUMES}"
    "-DDUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}"
    "-DDUNYA_SAMPLER_LINEAR_REPEAT=${DUNYA_SAMPLER_LINEAR_REPEAT}"
    "-DDUNYA_SAMPLER_LINEAR_CLAMP=${DUNYA_SAMPLER_LINEAR_CLAMP}"
    "-DDUNYA_SAMPLER_NEAREST_CLAMP=${DUNYA_SAMPLER_NEAREST_CLAMP}"
)
set(SHADER_INCLUDE_FLAG "-I${SHADER_SOURCE_DIR}")

file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.glsl")
string(JOIN " " SHADER_DEFINES_ARGS ${SHADER_DEFINES})
set(TEXTURE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/textures")
set(MODEL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm
  GIT_TAG        0af55ccecd98d4e5a8d1fad7de25ba429d60e863
)
FetchContent_Declare(
    stb
    GIT_REPOSITORY  https://github.com/nothings/stb
    GIT_TAG         31c1ad37456438565541f4919958214b6e762fb4
)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY  https://github.com/tinyobjloader/tinyobjloader
    GIT_TAG         e60d33385e2e4f7fa891513150f2532b5bbcb093
    SOURCE_SUBDIR  cmake_disabled
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY  https://github.com/ocornut/imgui
    GIT_TAG         v1.91.9b
    SOURCE_SUBDIR   cmake_disabled
)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
set(USE_ASSERTS ON)
set(OVERRIDE_CXX_FLAGS OFF)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY  https://github.com/jrouwe/JoltPhysics
    GIT_TAG         v5.6.0
    SOURCE_SUBDIR   Build
)
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY  https://github.com/skypjack/entt
    GIT_TAG         v4.0.0
)
FetchContent_Declare(
    glaze
    GIT_REPOSITORY  https://github.com/stephenberry/glaze
    GIT_TAG         v8.3.0
)
FetchContent_MakeAvailable(glfw glm stb tinyobjloader imgui JoltPhysics EnTT glaze)
find_package(Vulkan REQUIRED)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${imgui_SOURCE_DIR}/misc/cpp"
)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
target_compile_features(imgui PUBLIC cxx_std_20)
add_subdirectory(src/dunya/core)
add_subdirectory(src/dunya/objectmodel)
add_subdirectory(src/dunya/editor)
add_subdirectory(src/dunya/platform)
add_subdirectory(src/dunya/field)
add_subdirectory(src/dunya/physics)
add_subdirectory(src/dunya/serialize)
add_subdirectory(src/dunya/runtime)
add_subdirectory(src/dunya/imagecompare)
enable_testing()
add_executable(DunyaEditor
    src/main.cc
    src/capture/capture.cc
    src/overlay/overlay.cc
    src/startupoptions/startupoptions.cc
    src/framecheck/framecheck.cc
    src/demodriver/demodriver.cc

    src/debugtools/debugtools.cc
    src/cameracontroller/cameracontroller.cc
    src/application/application.cc
    src/scene/scene.cc
)
find_program(
    GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)
add_subdirectory(src/dunya/gpu)
add_subdirectory(src/dunya/renderer)
target_compile_definitions(DunyaEditor PRIVATE
    SHADER_SOURCE_DIR=\"${SHADER_SOURCE_DIR}\"
    GLSLC_PATH=\"${GLSLC_EXECUTABLE}\"
    GLSLC_DEFINES=\"${SHADER_DEFINES_ARGS}\"
    GLSLC_INCLUDE_DIR=\"${SHADER_SOURCE_DIR}\"
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
            ${SHADER_DEFINES}
            "${SHADER_INCLUDE_FLAG}"
            "${SHADER_SOURCE}"
            -o "${SHADER_OUTPUT}"

        DEPENDS "${SHADER_SOURCE}" ${SHADER_INCLUDES}
        COMMENT "Compiling shader ${SHADER_FILENAME}"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()

add_custom_target(
    SHADER_TARGET
    DEPENDS ${SHADER_OUTPUTS}
)

add_dependencies(DunyaEditor SHADER_TARGET)
add_executable(DunyaRuntime
    src/runtime/main.cc
    src/capture/capture.cc
    src/startupoptions/startupoptions.cc
    src/framecheck/framecheck.cc
    src/demodriver/demodriver.cc
    src/cameracontroller/cameracontroller.cc
    src/application/application.cc
    src/scene/scene.cc
)
add_dependencies(DunyaRuntime SHADER_TARGET)
if (MSVC)
    target_compile_options(DunyaRuntime PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(DunyaRuntime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(DunyaRuntime PRIVATE
    dunya::serialize
    glfw
    Vulkan::Vulkan
    glm::glm
    dunya::core
    dunya::objectmodel
    dunya::platform
    dunya::gpu
    dunya::renderer
    dunya::field
    dunya::physics
    dunya::runtime
    dunya::imagecompare
)

target_include_directories(DunyaRuntime PRIVATE src ${stb_SOURCE_DIR} ${tinyobjloader_SOURCE_DIR})
if (MSVC)
    target_compile_options(DunyaEditor PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(DunyaEditor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_compile_definitions(DunyaEditor PRIVATE DUNYA_EDITOR=1)
target_link_libraries(DunyaEditor PRIVATE
    dunya::serialize
    glfw
    Vulkan::Vulkan
    glm::glm
    dunya::core
    dunya::objectmodel
    dunya::editor
    dunya::platform
    dunya::gpu
    dunya::renderer
    dunya::field
    dunya::physics
    dunya::runtime
    dunya::imagecompare
    imgui
)
target_include_directories(DunyaEditor PRIVATE src ${stb_SOURCE_DIR} ${tinyobjloader_SOURCE_DIR})

foreach(DUNYA_APP DunyaEditor DunyaRuntime)
    add_custom_command(
        TARGET ${DUNYA_APP} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:${DUNYA_APP}>/shaders
        COMMENT "Copying shaders"
        VERBATIM
    )
    add_custom_command(
        TARGET ${DUNYA_APP} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${TEXTURE_SOURCE_DIR} $<TARGET_FILE_DIR:${DUNYA_APP}>/textures
        COMMENT "Copying textures"
        VERBATIM
    )
    add_custom_command(
        TARGET ${DUNYA_APP} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${MODEL_SOURCE_DIR} $<TARGET_FILE_DIR:${DUNYA_APP}>/models
        COMMENT "Copying models"
        VERBATIM
    )
endforeach()
add_subdirectory(tests)
