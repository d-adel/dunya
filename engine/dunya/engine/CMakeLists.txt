add_library(dunya_engine STATIC
    engine/engine.cc
)
add_library(dunya::engine ALIAS dunya_engine)
target_include_directories(dunya_engine PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_compile_features(dunya_engine PUBLIC cxx_std_23)

target_link_libraries(dunya_engine PUBLIC
    dunya::assets
    dunya::core
    dunya::field
    dunya::gpu
    dunya::objectmodel
    dunya::physics
    dunya::renderer
    dunya::runtime
    dunya::script
    dunya::serialize
    dunya::systems
)

if (MSVC)
    target_compile_options(dunya_engine PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_engine Runtime)
