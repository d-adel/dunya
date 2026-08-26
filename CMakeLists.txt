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

# Capacities the C++ side and the shaders must agree on to the number. They live
# here rather than in config.h because this is the only place that can reach
# both compilers; drift between them does not fail to build, it corrupts an
# std140 stride or indexes past an array.
set(DUNYA_MAX_FIELD_PRIMITIVES 128)
set(DUNYA_MAX_TEXTURES 16)
set(DUNYA_MAX_SAMPLERS 8)
set(DUNYA_MAX_MATERIALS 64)
set(DUNYA_MAX_FIELD_OBJECTS 64)

# Marching parameters. These are *defaults*, not shared declarations: the shader
# reads them from a uniform (MarchParams) so they can be turned at runtime, and
# what CMake still owns is the value they start at - one number in one place,
# handed to the CPU only. The CPU still needs them because a CPU query and the
# drawn image have to agree about where a surface is, or a click carves
# somewhere other than where the crosshair pointed.
set(DUNYA_MARCH_EPSILON 0.001f)
set(DUNYA_MARCH_MAX_ITERATIONS 800)
set(DUNYA_SHADOW_MAX_DISTANCE 20.0f)
set(DUNYA_MARCH_MAX_DISTANCE 100.0f)
set(DUNYA_GRADIENT_EPSILON 0.01f)

# Over-relaxation multiplier for sphere tracing. Helps most when the distance
# estimator is conservative, which subtraction's max(a, -b) makes ours.
set(DUNYA_MARCH_OMEGA 1.2f)
set(DUNYA_SHADOW_SHARPNESS 12.0f)

# Trilinear interpolation of a distance field overestimates - the sampled tests
# measure it - so a march through the grid shortens every step rather than
# trusting a value that is not a true bound.
set(DUNYA_GRID_STEP_SAFETY 0.8f)

# Sampler slots, shared so the shader does not have to guess the order Scene
# creates them in.
set(DUNYA_SAMPLER_LINEAR_REPEAT 0)
set(DUNYA_SAMPLER_LINEAR_CLAMP 1)
set(DUNYA_SAMPLER_NEAREST_CLAMP 2)

# Only what *declares* something both languages must agree on: array capacities
# and the slot numbers Scene fills. Anything tunable is deliberately absent - it
# reaches the shader as a uniform now, and leaving it here as well would put the
# same number back in two toolchains, which is the thing idiom 27 is about.
set(SHADER_DEFINES
    "-DDUNYA_MAX_FIELD_PRIMITIVES=${DUNYA_MAX_FIELD_PRIMITIVES}"
    "-DDUNYA_MAX_TEXTURES=${DUNYA_MAX_TEXTURES}"
    "-DDUNYA_MAX_SAMPLERS=${DUNYA_MAX_SAMPLERS}"
    "-DDUNYA_MAX_MATERIALS=${DUNYA_MAX_MATERIALS}"
    "-DDUNYA_MAX_FIELD_OBJECTS=${DUNYA_MAX_FIELD_OBJECTS}"
    "-DDUNYA_SAMPLER_LINEAR_REPEAT=${DUNYA_SAMPLER_LINEAR_REPEAT}"
    "-DDUNYA_SAMPLER_LINEAR_CLAMP=${DUNYA_SAMPLER_LINEAR_CLAMP}"
    "-DDUNYA_SAMPLER_NEAREST_CLAMP=${DUNYA_SAMPLER_NEAREST_CLAMP}"
)

# Kept out of the flag string above because the runtime hot-reload path has to
# quote this itself: a path with a space in it would otherwise be read as two
# arguments and glslc would report "linking multiple files is not supported".
set(SHADER_INCLUDE_FLAG "-I${SHADER_SOURCE_DIR}")

# Shared GLSL is not compiled on its own, but every shader that includes it has
# to rebuild when it changes, so it joins their dependencies explicitly.
file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.glsl")

# The same flags as one string, so the runtime hot-reload path can pass exactly
# what the build passed. A reloaded shader compiled with different capacities
# would disagree with the buffers already bound to it.
string(JOIN " " SHADER_DEFINES_ARGS ${SHADER_DEFINES})

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

# Fetch Dear ImGui. Like stb it is a source drop with no build system of its
# own, so SOURCE_SUBDIR points at nothing and the library target is declared
# below rather than inherited.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY  https://github.com/ocornut/imgui
    GIT_TAG         v1.91.9b
    SOURCE_SUBDIR   cmake_disabled
)

# Fetch Jolt Physics. Its CMakeLists lives in Build/ rather than at the repo
# root, and it disables its own samples and tests when it is not top level.
# OFF because Jolt defaults it ON and would build against /MTd while every
# other target here uses CMake's default /MDd - objects from the two runtimes
# cannot be linked.
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)

# Jolt compiles its preconditions out by default, which turns a misuse into a
# bare segfault instead of a message. Debug builds want them.
set(USE_ASSERTS ON)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY  https://github.com/jrouwe/JoltPhysics
    GIT_TAG         v5.6.0
    SOURCE_SUBDIR   Build
)

FetchContent_MakeAvailable(glfw glm stb tinyobjloader imgui JoltPhysics)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# The overlay is a development tool built as its own target, and deliberately
# not held to /W4 /WX: third-party code should not have to meet this project's
# warning bar, and its own target is the cleanest place to say so - no
# per-source overrides and no pragmas wrapped around includes.
#
# Declared after find_package because it links Vulkan::Vulkan, which does not
# exist until then.
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

target_include_directories(imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

target_compile_features(imgui PUBLIC cxx_std_20)

# Dunya libraries
add_subdirectory(src/dunya/core)
add_subdirectory(src/dunya/objectmodel)
add_subdirectory(src/dunya/editor)
add_subdirectory(src/dunya/platform)
add_subdirectory(src/dunya/field)
add_subdirectory(src/dunya/physics)

# Image comparison, free of Vulkan for the same reason
add_subdirectory(src/dunya/imagecompare)

enable_testing()

# Define executable and link libraries
add_executable(${PROJECT_NAME}
    src/main.cc
    src/capture/capture.cc
    src/overlay/overlay.cc
    src/startupoptions/startupoptions.cc
    src/framecheck/framecheck.cc
    src/application/application.cc
    src/scene/scene.cc
    src/physicsdemo/physicsdemo.cc
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

# Configuration
# The GLM_* defines are not repeated here: they arrive as usage requirements
# from dunya::field, which is the only way both sides are guaranteed to agree
# on glm's type layout.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_SOURCE_DIR=\"${SHADER_SOURCE_DIR}\"
    GLSLC_PATH=\"${GLSLC_EXECUTABLE}\"
    GLSLC_DEFINES=\"${SHADER_DEFINES_ARGS}\"
    GLSLC_INCLUDE_DIR=\"${SHADER_SOURCE_DIR}\"

    # The starting values for MarchParams. Three of these reached only the
    # shader before, which was fine while the shader was the only reader; now
    # that the C++ side owns them and uploads them, all of them come here.
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

add_dependencies(${PROJECT_NAME} SHADER_TARGET)

# Target
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE
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

    # Also where stb's implementations now live, for the whole project: texture
    # loading gets them from here rather than from a second copy that would
    # collide at link time.
    dunya::imagecompare
    imgui
)
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

# After the executable, not before it: the golden image tests run the renderer
# itself, so the target has to exist before a test can name it.
add_subdirectory(tests)
