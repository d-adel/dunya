add_library(dunya_assets STATIC
    assetlibrary/assetlibrary.cc
)
add_library(dunya::assets ALIAS dunya_assets)
target_include_directories(dunya_assets PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_compile_features(dunya_assets PUBLIC cxx_std_23)

target_link_libraries(dunya_assets PUBLIC
    dunya::core
    dunya::gpu
    dunya::objectmodel
    dunya::renderer
    dunya::serialize
)

if (MSVC)
    target_compile_options(dunya_assets PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_assets PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_assets Runtime)
