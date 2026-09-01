cmake_minimum_required(VERSION 3.28)

project(Dunya VERSION 0.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(DunyaModule)

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
set(DUNYA_MAX_SDF_PRIMITIVES 128)
set(DUNYA_MAX_TEXTURES 16)
set(DUNYA_MAX_SAMPLERS 8)
set(DUNYA_MAX_MATERIALS 64)
set(DUNYA_MAX_SDF_RECORDS 1280)
set(DUNYA_MAX_SDF_VOLUMES 1280)

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

set(DUNYA_SHAPE_SPHERE 0)
set(DUNYA_SHAPE_BOX 1)
set(DUNYA_SHAPE_PLANE 2)
set(DUNYA_SHAPE_CYLINDER 3)
set(DUNYA_CSG_UNION 0)
set(DUNYA_CSG_SMOOTH_UNION 1)
set(DUNYA_CSG_INTERSECTION 2)
set(DUNYA_CSG_SUBTRACTION 3)
set(DUNYA_CSG_SMOOTH_SUBTRACTION 4)
set(SHADER_DEFINES
    "-DDUNYA_MAX_SDF_PRIMITIVES=${DUNYA_MAX_SDF_PRIMITIVES}"
    "-DDUNYA_MAX_TEXTURES=${DUNYA_MAX_TEXTURES}"
    "-DDUNYA_MAX_SAMPLERS=${DUNYA_MAX_SAMPLERS}"
    "-DDUNYA_MAX_MATERIALS=${DUNYA_MAX_MATERIALS}"
    "-DDUNYA_MAX_SDF_RECORDS=${DUNYA_MAX_SDF_RECORDS}"
    "-DDUNYA_MAX_SDF_VOLUMES=${DUNYA_MAX_SDF_VOLUMES}"
    "-DDUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}"
    "-DDUNYA_SAMPLER_LINEAR_REPEAT=${DUNYA_SAMPLER_LINEAR_REPEAT}"
    "-DDUNYA_SAMPLER_LINEAR_CLAMP=${DUNYA_SAMPLER_LINEAR_CLAMP}"
    "-DDUNYA_SAMPLER_NEAREST_CLAMP=${DUNYA_SAMPLER_NEAREST_CLAMP}"
    "-DDUNYA_SHAPE_SPHERE=${DUNYA_SHAPE_SPHERE}"
    "-DDUNYA_SHAPE_BOX=${DUNYA_SHAPE_BOX}"
    "-DDUNYA_SHAPE_PLANE=${DUNYA_SHAPE_PLANE}"
    "-DDUNYA_SHAPE_CYLINDER=${DUNYA_SHAPE_CYLINDER}"
    "-DDUNYA_CSG_UNION=${DUNYA_CSG_UNION}"
    "-DDUNYA_CSG_SMOOTH_UNION=${DUNYA_CSG_SMOOTH_UNION}"
    "-DDUNYA_CSG_INTERSECTION=${DUNYA_CSG_INTERSECTION}"
    "-DDUNYA_CSG_SUBTRACTION=${DUNYA_CSG_SUBTRACTION}"
    "-DDUNYA_CSG_SMOOTH_SUBTRACTION=${DUNYA_CSG_SMOOTH_SUBTRACTION}"
)
set(SHADER_INCLUDE_FLAG "-I${SHADER_SOURCE_DIR}")

file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.glsl")
string(JOIN " " SHADER_DEFINES_ARGS ${SHADER_DEFINES})
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
FetchContent_MakeAvailable(glfw glm stb tinyobjloader JoltPhysics EnTT glaze)

target_compile_definitions(glm INTERFACE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)

find_package(Vulkan REQUIRED)

add_subdirectory(engine/dunya/core)
add_subdirectory(engine/dunya/objectmodel)
add_subdirectory(engine/dunya/editor)
add_subdirectory(engine/dunya/viewport)
add_subdirectory(engine/dunya/platform)
add_subdirectory(engine/dunya/field)
add_subdirectory(engine/dunya/physics)
add_subdirectory(engine/dunya/serialize)
add_subdirectory(engine/dunya/runtime)
add_subdirectory(engine/dunya/systems)
add_subdirectory(engine/dunya/script)
add_subdirectory(engine/dunya/stb)
add_subdirectory(engine/dunya/imagecompare)
enable_testing()
find_program(
    GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)
add_subdirectory(engine/dunya/gpu)
add_subdirectory(engine/dunya/renderer)
add_subdirectory(engine/dunya/assets)
add_subdirectory(engine/dunya/engine)
add_subdirectory(engine/capi)
add_subdirectory(editor)
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

add_executable(DunyaPlayer
    engine/player/main.cc
    engine/app/capture/capture.cc
    engine/app/startupoptions/startupoptions.cc
    engine/app/framecheck/framecheck.cc
    engine/app/inputbridge/inputbridge.cc
    engine/app/glfwwindowsystem/glfwwindowsystem.cc
    engine/app/engineloop/engineloop.cc
)
add_dependencies(DunyaPlayer SHADER_TARGET)
if (MSVC)
    target_compile_options(DunyaPlayer PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(DunyaPlayer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(DunyaPlayer PRIVATE
    dunya::assets
    dunya::engine
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
    dunya::script
    dunya::systems
    dunya::imagecompare
)

target_include_directories(DunyaPlayer PRIVATE engine ${stb_SOURCE_DIR} ${tinyobjloader_SOURCE_DIR})
add_custom_target(DUNYA_CONTENT ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:DunyaPlayer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/projects $<TARGET_FILE_DIR:DunyaPlayer>/projects
    COMMENT "Copying shaders and projects"
    VERBATIM
)
add_dependencies(DUNYA_CONTENT DunyaPlayer SHADER_TARGET)
add_executable(DunyaPackager
    engine/tools/packager/main.cc
)
target_link_libraries(DunyaPackager PRIVATE dunya::editor)
if (MSVC)
    target_compile_options(DunyaPackager PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(DunyaPackager PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(DunyaPackager Editor)
dunya_module(DunyaPlayer Developer)

add_subdirectory(tests)

dunya_require_declared()
dunya_check_modules()
