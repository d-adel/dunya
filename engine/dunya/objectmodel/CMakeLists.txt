add_library(dunya_objectmodel STATIC
    instantiate/instantiate.cc
    defaultworld/defaultworld.cc
    component/renderpose/renderpose.cc
    component/sdfgrid/sdfgrid.cc
    sdfprimitivestore/sdfprimitivestore.cc
    dynamiccomponents/dynamiccomponents.cc
    world/world.cc
    worldcontents/worldcontents.cc
    worldquery/worldquery.cc
)
add_library(dunya::objectmodel ALIAS dunya_objectmodel)
target_include_directories(dunya_objectmodel PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_objectmodel PUBLIC cxx_std_23)
target_link_libraries(dunya_objectmodel PUBLIC
    dunya::core
    dunya::field
    glm::glm
    EnTT::EnTT
)
if (MSVC)
    target_compile_options(dunya_objectmodel PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_objectmodel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_objectmodel Runtime)
