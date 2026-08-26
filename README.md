# dunya

dunya is a game engine written in C++ focused on interactive, deformable worlds.

To make that possible, dunya allows objects to use either conventional geometry
or field-based geometry. Field-based objects describe their shape through values
defined throughout 3D space, rather than only through an explicit surface.
The field can then be evaluated at arbitrary positions, which makes the geometry
directly useful for rendering, modification, and geometric queries used by
physics.

The field representation currently used by dunya is the signed distance field
(SDF), where each query returns a signed distance to the surface. SDFs can be
built analytically from primitives and Constructive Solid Geometry (CSG)
operations, or sampled onto a 3D grid.
The analytic form is useful for constructing and describing an object,
while the sampled form gives dunya a persistent representation that can be
modified locally during runtime.
The renderer can use that field to find the visible surface, while physics can
query the same geometric information for collision and contact calculations.

This means that changing the field changes the object itself.
Operations applied to fields do not need to produce an entirely separate
representation for each engine system, and the modified shape can remain the
geometry that those systems work from.

![dunya carving a field object in real time](screenshots/dunya.gif)

## Why I am building it

I am building dunya because I want to make games with worlds that can respond to the player in many different ways and support new types of interaction. I also like the idea that someone much more creative than me could eventually use the same systems to make something I would never have thought of.

Building the engine myself gives me control over how its systems fit together and lets me shape the technology around those kinds of games. A big part of the project is also just learning  (which I've already done an unreasonable amount of) from this thing, and I am nowhere near done.

## What it is

dunya is currently made up of nine libraries and one application that brings
them together. Each library is its own build target, and a library is only
allowed to depend on the ones below it.

- `dunya::field` is the field system itself. It holds the primitives and the CSG
  operations that combine them into an analytic field, the baking step that
  samples that field onto a 3D grid, and the ray casting used against both
  forms.
- `dunya::objectmodel` describes what exists in a world and where it is. It
  holds the field objects and their primitives, the registry that stores them,
  the world that also carries the meshes to be drawn, and supporting things like
  cameras and materials.
- `dunya::renderer` turns a world into a rendered frame. It contains the field
  marching, the mesh drawing, the per-frame data the shaders read, and the pool
  that keeps baked fields as 3D textures on the GPU.
- `dunya::editor` contains the operations that change a world, such as placing
  an object or carving into one. Every operation is a command that knows how to
  undo itself, so the editor can move backwards and forwards through them.
- `dunya::physics` is the rigid body simulation, which comes from Jolt and runs
  at a fixed timestep. Jolt is used here and nowhere else in the engine.
- `dunya::gpu` is a thin layer over Vulkan, covering devices, swapchains,
  buffers, images, samplers, pipelines and descriptor sets. It does not know
  what a field is, which is what lets it stay a general rendering backend.
- `dunya::platform` handles the window and the keyboard and mouse input, which
  currently comes from GLFW.
- `dunya::core` holds the shared configuration values and the event system that
  the other libraries agree on.
- `dunya::imagecompare` loads and saves images and compares two of them. The
  renderer uses it for textures, and the tests use it to check rendered frames
  against reference images.

The object model sits in the middle of all this, and that is what keeps the rest
of it separate. Because it describes objects in plain data, the renderer, the
editor and the physics can each read and write the same world without needing to
know anything about each other. The field library sits next to it and provides
the shared description of shape, while the gpu and platform libraries sit
underneath and know nothing about either. The application is the only place
where all of it comes together.

## Current status

What currently works:

- A Vulkan renderer using dynamic rendering and synchronization2, with frames in
  flight and window resizing.
- Mesh rendering from OBJ files, with textures, depth and diffuse lighting.
- Analytic SDF ray marching, with primitives combined through CSG operations.
- Meshes and fields drawn in the same scene
- The same field evaluated on the CPU from the same primitive data the GPU
  reads, which is what makes carving and geometric queries possible.
- Interactive editing, where a click carves into an object or adds to it.
- Sampled fields baked onto a grid per object in the object's own space, so a
  carved object can be moved and rotated and keep its geometry. Analytic and
  sampled rendering can be switched at runtime.
- A material table shared by both the mesh path and the field path.
- An object registry and a world, with editor commands that can be undone and
  redone.
- A Dear ImGui overlay for inspecting and changing things while running.
- Golden image regression tests, so a refactor that changes the picture is
  caught rather than noticed later.
- Jolt built into the project and running a fixed timestep simulation, with a
  test that checks the same starting conditions produce the same trajectory
  exactly.

Currently working on:

- Splitting the editor world from the runtime world, so that we get a
  a separate world to simulate.
- Creating physics bodies from the objects in a world
- A collision shape backed by the field itself, so contacts come from the
  object's real geometry.
- Making sampled fields mutable, so an impact can dent an object and the dent
  stays.

## How I use AI

Most lines in `src/` are written by myself because I'm building this to
understand how these systems actually work.
When I don't something I ask, and then I go and write it myself.
Things I do understand but require a lot of scaffolding I let Claude write as
well (you can probably tell from the verbose commenting). Regardless, I review
extensively and test in runtime as much as needed.

Additionally, I let Claude do the work around the code.
It reviews my changes and tells me when something is wrong.
It builds and runs the project against the criteria I set for each milestone
and it answers questions about C++ and Vulkan while I am still learning them.
It also writes the unit tests and when needed it moves code around without
rewriting anything. However, decisionmaking is only done by myself.

## Building

dunya builds from a clean clone with no manual dependency setup. Everything
except the Vulkan SDK is fetched by CMake.

You need:

- The [LunarG Vulkan SDK](https://vulkan.lunarg.com/). CMake finds it through
  the `VULKAN_SDK` environment variable that the installer sets, and it also
  provides `glslc`, which compiles the shaders as part of the build.
- CMake 3.28 or newer.
- A C++20 compiler. I build with Visual Studio 2026, and the warning settings
  are strict, so the build should be silent.
- Git (obviously)

Then:

```sh
git clone https://github.com/d-adel/dunya.git
cd dunya
cmake -S . -B build
cmake --build build
