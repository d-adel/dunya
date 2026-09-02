add_library(dunya_gizmos STATIC
    grid/grid.cc
)

add_library(dunya::gizmos ALIAS dunya_gizmos)
target_include_directories(dunya_gizmos PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_gizmos PUBLIC cxx_std_23)

target_link_libraries(dunya_gizmos PUBLIC
    dunya::core
    dunya::gpu
    dunya::view
)

if (MSVC)
    target_compile_options(dunya_gizmos PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_gizmos PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_gizmos Editor)
