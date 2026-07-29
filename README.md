# dunya-renderer

A Vulkan 1.4 renderer written from scratch in C++20 — dynamic rendering, no
`VkRenderPass`, `synchronization2` barriers written by hand, and one RAII
wrapper per Vulkan resource. No engine, no framework, no helper library over
the API: GLFW for the window, GLM for the math, and the raw C API for
everything else.

![Textured OBJ mesh with depth and diffuse lighting](screenshots/viking-room.png)

## Requirements

- **LunarG Vulkan SDK** (developed against 1.4.350) with `VULKAN_SDK` set. It
  is needed at build time for `glslc`, and at run time for the validation
  layers, which this project never disables.
- **CMake ≥ 3.28**
- **A C++20 compiler** — MSVC (VS 2022 or newer) is what this is built and
  tested with.
- **A 64-bit target.** On a 32-bit target `VK_NULL_HANDLE` expands to `0ULL`
  rather than `nullptr`, and assigning it to a dispatchable handle does not
  compile. Configure with `-A x64` (or select an `amd64` kit in your IDE).
- A GPU with Vulkan 1.3 core features `dynamicRendering` and
  `synchronization2`, plus `samplerAnisotropy`. Device selection rejects
  anything that lacks them.

GLFW 3.4, GLM, stb and tinyobjloader are fetched automatically by CMake —
there is nothing to install or vendor by hand, and a clean clone builds with
no further setup.

## Build and run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
./build/Debug/DunyaRenderer.exe
```

Shaders are compiled to SPIR-V by `glslc` as a CMake build step (never by
hand), and the compiled shaders, textures and models are copied next to the
executable after every build, so the program must be run from its own output
directory.

## Credits and references

The order things are built in follows
[vulkan-tutorial.com](https://vulkan-tutorial.com/) — instance, devices,
swapchain, pipeline, buffers, descriptors, textures, depth, model — while the
modern parts that tutorial predates, dynamic rendering and `synchronization2`,
follow the current [Khronos Vulkan
Tutorial](https://docs.vulkan.org/tutorial/latest/) instead. Sascha Willems'
samples served as a reference for the raw C API.

What does not come from the tutorial is the ownership layer. The tutorial
keeps every handle in one large class and tears them down by hand in a
`cleanup()` function; here each resource gets its own RAII wrapper, with copy
and move policy decided at declaration, hand-written moves only in the classes
that own raw handles, and destruction driven entirely by scope. That design,
and the C++ it is made of, is mine. The code is written by hand rather than
copied — the writing is the point of the project.

The scene model, `models/viking_room.obj` with `textures/viking_room.png`, is
[Viking room](https://sketchfab.com/3d-models/viking-room-a49f1b8e4f5c4ecf9e1fe7d81915ad38)
by **nigelgoh**, licensed [CC BY
4.0](https://creativecommons.org/licenses/by/4.0/) and used here in the form
distributed with the Vulkan tutorial.
