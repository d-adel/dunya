add_library(dunya_c SHARED
    dunya_c.cc
    session/session.cc
    win32windowsystem/win32windowsystem.cc
)
add_library(dunya::capi ALIAS dunya_c)
target_include_directories(dunya_c PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_compile_features(dunya_c PUBLIC cxx_std_23)

target_link_libraries(dunya_c PRIVATE
    dunya::assets
    dunya::core
    dunya::field
    dunya::gpu
    dunya::objectmodel
    dunya::renderer
    dunya::physics
    dunya::runtime
    dunya::serialize
    dunya::script
    dunya::systems
    dunya::viewport
)

if (MSVC)
    target_compile_options(dunya_c PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_c PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_c Editor)
