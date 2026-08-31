add_library(dunya_viewport STATIC
    camera/camera.cc
    grid/grid.cc
)

add_library(dunya::viewport ALIAS dunya_viewport)
target_include_directories(dunya_viewport PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_viewport PUBLIC cxx_std_23)

target_link_libraries(dunya_viewport PUBLIC
    dunya::core
    dunya::objectmodel
    dunya::gpu
)

if (MSVC)
    target_compile_options(dunya_viewport PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_viewport PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_viewport Developer)
