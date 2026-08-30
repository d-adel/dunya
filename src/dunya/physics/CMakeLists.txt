add_library(dunya_physics STATIC
   fieldshape/fieldshape.cc
   impact/impact.cc
   joltlibrary/joltlibrary.cc
   layers/layers.cc
   physicsworld/physicsworld.cc
)
add_library(dunya::physics ALIAS dunya_physics)
target_include_directories(dunya_physics PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(dunya_physics PUBLIC cxx_std_23)
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
