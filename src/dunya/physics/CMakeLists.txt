# Rigid-body simulation over a world. Jolt lives here and nowhere else, so a
# target that wants an undo stack or a unit test does not link a solver.
# Vulkan-free by construction: contacts read the CPU grid, never the GPU one.

add_library(dunya_physics STATIC
   joltlibrary/joltlibrary.cc
   layers/layers.cc
   physicsworld/physicsworld.cc
)

add_library(dunya::physics ALIAS dunya_physics)

target_include_directories(dunya_physics PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_physics PUBLIC cxx_std_20)

# PUBLIC: Jolt types and a World reference both appear in headers consumers
# include. field comes through objectmodel already, listed for the same reason
# the editor lists it - the dependency is real, not incidental.
target_link_libraries(dunya_physics PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
    Jolt
)

if (MSVC)
    target_compile_options(dunya_physics PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_physics PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
