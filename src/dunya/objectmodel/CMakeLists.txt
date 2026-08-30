add_library(dunya_objectmodel STATIC
    camera/camera.cc
    instantiate/instantiate.cc
    sdfgrid/sdfgrid.cc
    sdfprimitivestore/sdfprimitivestore.cc
    world/world.cc
)
add_library(dunya::objectmodel ALIAS dunya_objectmodel)
target_include_directories(dunya_objectmodel PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(dunya_objectmodel PUBLIC cxx_std_20)
target_link_libraries(dunya_objectmodel PUBLIC
    dunya::core
    dunya::field
    glm::glm
    EnTT::EnTT
)
if (MSVC)
    target_compile_options(dunya_objectmodel PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_objectmodel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
