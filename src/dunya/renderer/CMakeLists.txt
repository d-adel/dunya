# Where the object model becomes pixels. This is the only library that sees
# both Vulkan and the field, which is what keeps dunya::gpu an RHI.

add_library(dunya_renderer STATIC
    renderer.cc
    fieldbaker/fieldbaker.cc
    fieldobjecttable/fieldobjecttable.cc
    fieldrecord/fieldrecord.cc
    frame/frame.cc
    frameglobals/frameglobals.cc
    mesh/mesh.cc
    mesh/tiny_obj_loader_impl.cc
    resourcetable/resourcetable.cc
    vertex/vertex.cc
    volumepool/volumepool.cc
)

add_library(dunya::renderer ALIAS dunya_renderer)

target_include_directories(dunya_renderer PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_include_directories(dunya_renderer PRIVATE "${tinyobjloader_SOURCE_DIR}")

target_compile_features(dunya_renderer PUBLIC cxx_std_20)

target_link_libraries(dunya_renderer PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
    dunya::gpu
)

# The three march parameters the field library does not already publish.
# frame.h reads them and consumers include frame.h, so PUBLIC - and no other
# target declares them, which is idiom 27 held by the link graph.
target_compile_definitions(dunya_renderer PUBLIC
    DUNYA_SHADOW_MAX_DISTANCE=${DUNYA_SHADOW_MAX_DISTANCE}
    DUNYA_SHADOW_SHARPNESS=${DUNYA_SHADOW_SHARPNESS}
)

if (MSVC)
    target_compile_options(dunya_renderer PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_renderer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
