# What a scene is made of, with no idea how it is drawn. Vulkan-free by
# construction: this is the library physics, the editor and the tests all
# reach for, and none of them should have to link a renderer to get it.

add_library(dunya_objectmodel STATIC
    camera/camera.cc
    fieldobject/fieldobject.cc
    sdfprimitivestore/sdfprimitivestore.cc
    world/world.cc
)

add_library(dunya::objectmodel ALIAS dunya_objectmodel)

target_include_directories(dunya_objectmodel PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_objectmodel PUBLIC cxx_std_20)

# PUBLIC: FieldObject and Command carry glm and Primitive types in headers
# consumers include, and Entity is an EnTT type. EnTT stops here on purpose:
# field, gpu and platform have no entities and do not link this.
target_link_libraries(dunya_objectmodel PUBLIC
    dunya::core
    dunya::field
    glm::glm
    EnTT::EnTT
)

if (MSVC)
    target_compile_options(dunya_objectmodel PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_objectmodel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
