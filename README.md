# dunya

dunya is a game engine for deformable worlds.

![192 crates being shot apart in real time, each one a signed distance field that is deformed by the impacts and collided against](screenshots/dunya.gif)

Objects in dunya are signed distance fields sampled onto a grid, rather than
triangle meshes. The renderer marches that grid, physics collides against it,
and an impact writes into it, so damage is the geometry rather than a copy of
it. Meshes are still supported and run on their own pipeline.

## Features

### Fields

- Analytic SDFs built from primitives and CSG operations
- Sampled fields baked onto a per-object grid, in the object's own space
- The same field evaluated on the CPU and on the GPU, from the same data
- Per-brick value ranges and Lipschitz bounds, reduced by a compute shader
- Local writes with a narrow-band redistancing sweep, so a dent costs the dent
- Copy on write, so undamaged objects share one lattice

### Rendering

- Vulkan 1.4, dynamic rendering, synchronization2, frames in flight, resizing
- Sphere tracing against both forms, switchable at runtime
- Mesh rendering from OBJ, with textures, depth and diffuse lighting
- One material table shared by the field path and the mesh path
- Shadow casters binned across the light, so the shadow term stops scaling with
  object count
- Golden image regression tests

### Physics

- Jolt at a fixed timestep, deterministic
- A Jolt collision shape backed by the field itself, contacts seeded from
  surface bricks
- Continuous collision by conservative advancement over the field
- Mass, centre of mass and inertia integrated from the field
- Impacts written back into the field as craters
- A thousand bodies at 84 fps settling, 71 fps under two craters a second

### System

- C++23, `/W4 /WX`, zero warnings
- Eleven libraries, each its own target, each depending only on the ones below it
- EnTT object model; worlds, materials and assets are JSON on disk
- Editor and runtime are separate executables, and the runtime links no editor
  code
- Dear ImGui panels for inspecting and changing things while running

## Layout

| | |
| --- | --- |
| `engine/` | the C++ engine: the `dunya::` libraries, the shared app shell, both executables |
| `editor/` | the C# editor, Avalonia on .NET 10 (in progress) |
| `projects/` | engine data: worlds, materials, meshes, textures |
| `shaders/` | GLSL, compiled by `glslc` as a build step |
| `tests/` | Catch2 unit tests and the golden image tests |

The libraries, bottom to top:

| | |
| --- | --- |
| `dunya::core` | shared configuration, events, telemetry |
| `dunya::field` | primitives, CSG, baking, ray casting, redistancing, deformation |
| `dunya::platform` | window, keyboard and mouse, currently GLFW |
| `dunya::imagecompare` | image load, save and compare |
| `dunya::gpu` | Vulkan: devices, swapchains, buffers, images, samplers, pipelines, descriptors |
| `dunya::objectmodel` | what exists in a world and where it is |
| `dunya::serialize` | the on-disk format for projects, worlds and materials |
| `dunya::physics` | Jolt, the field collision shape, impacts |
| `dunya::editor` | the operations that change a world, each one undoable |
| `dunya::renderer` | worlds into frames: marching, mesh drawing, volume residency, shadows |
| `dunya::runtime` | play mode: instantiation, bodies, deformation |

`dunya::gpu` does not know what a field is and `dunya::objectmodel` does not
know how anything is drawn, which is what keeps the renderer, the editor and the
physics able to read and write the same world without knowing about each other.
The application is the only place all of it meets.

## Building

### Prerequisites

- The [LunarG Vulkan SDK](https://vulkan.lunarg.com/). CMake finds it through
  the `VULKAN_SDK` environment variable the installer sets, and it provides
  `glslc`, which compiles the shaders as part of the build.
- [CMake](https://cmake.org/) 3.28 or newer, and Ninja
- A C++23 compiler. I build with Visual Studio Build Tools 2022, and the warning
  settings are strict, so the build should be silent.
- [Git](https://git-scm.com/)
- The [.NET 10 SDK](https://dotnet.microsoft.com/en-us/download), for the editor
  only

Everything else is fetched by CMake, so the engine builds from a clean clone
with no manual dependency setup.

### Building

```sh
git clone https://github.com/d-adel/dunya.git
cd dunya
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

On Windows, run these from a Developer Command Prompt for VS, or from VS Code
with the CMake Tools extension, so that `cl.exe` and `ninja` are on `PATH`.

There is also a `release` preset. It is the only one a performance number should
ever come from.

## Running

Two executables build from the same application shell:

- `DunyaDevHost`, the development host: fly camera, ImGui panels, editing
- `DunyaRuntime`, the runtime, with no editor code on its link line

Both open `projects/demo` by default.

```
Click to fire at the wall, F to fire at its middle
Hold right mouse to look and fly (WASD/QE)
G resets the wall, F5 stops the simulation
Alt + click carves by hand once stopped
```

| | |
| --- | --- |
| `--project DIR` | project to open, default `projects/demo` |
| `--world NAME` | world to load, default `main` |
| `--analytic` | march the analytic field instead of the sampled one |
| `--demo FRAMES --demo-rate PER_SEC` | fire on a timer and exit, for measurement |
| `--export-project DIR` | write the live world back out |

## Why I am building it

I am building dunya because I want to make games with worlds that can respond to the player in many different ways and support new types of interaction. I also like the idea that someone much more creative than me could eventually use the same systems to make something I would never have thought of.

Building the engine myself gives me control over how its systems fit together and lets me shape the technology around those kinds of games. A big part of the project is also just learning  (which I've already done an unreasonable amount of) from this thing, and I am nowhere near done.

## LLM Usage

Claude is used in the following areas:

- unit tests
- code review, and building and running the project against each milestone's
  acceptance criteria
- build and CI configuration
- moving code between files without rewriting it
- research, and answering C++ and Vulkan questions while I am still learning them

Some milestones are also written by Claude, under an exception I open and close
deliberately. Each one is recorded in `CLAUDE.md` with its scope, its end
condition, and the milestone it is not allowed to reach. Elsewhere the code is
written by me, and every design decision is mine either way.

## License

dunya uses the [MIT license](LICENSE).
