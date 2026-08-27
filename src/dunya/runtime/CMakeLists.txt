# What an authored world becomes when Play is pressed: a second world, the
# bodies simulating it, and the per-object state that only exists while it runs.
# No renderer here - a volume index is a number, and only allocating one needs
# to know what it indexes.

add_library(dunya_runtime STATIC
    runtime/runtime.cc
)

add_library(dunya::runtime ALIAS dunya_runtime)

target_include_directories(dunya_runtime PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_runtime PUBLIC cxx_std_20)

# PUBLIC: a World, a PhysicsWorld and a SampledField all appear in headers
# consumers include.
target_link_libraries(dunya_runtime PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
    dunya::physics
)

if (MSVC)
    target_compile_options(dunya_runtime PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
