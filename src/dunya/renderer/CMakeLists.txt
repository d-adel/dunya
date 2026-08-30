add_library(dunya_renderer STATIC
    renderer.cc
    fieldbaker/fieldbaker.cc
    fieldrecordtable/fieldrecordtable.cc
    fieldrecord/fieldrecord.cc
    frame/frame.cc
    frameglobals/frameglobals.cc
    meshbuffers/meshbuffers.cc
    meshbuffers/tiny_obj_loader_impl.cc
    resourcetable/resourcetable.cc
    vertex/vertex.cc
    shadowgrid/shadowgrid.cc
    fieldresidency/fieldresidency.cc
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
target_compile_definitions(dunya_renderer PUBLIC
    DUNYA_SHADOW_MAX_DISTANCE=${DUNYA_SHADOW_MAX_DISTANCE}
    DUNYA_SHADOW_SHARPNESS=${DUNYA_SHADOW_SHARPNESS}
)
if (MSVC)
    target_compile_options(dunya_renderer PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_renderer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
