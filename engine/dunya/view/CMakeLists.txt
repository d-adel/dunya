add_library(dunya_view STATIC
    camera/camera.cc
    viewportstore/viewportstore.cc
    lookthrough/lookthrough.cc
)

add_library(dunya::view ALIAS dunya_view)
target_include_directories(dunya_view PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_view PUBLIC cxx_std_23)

target_link_libraries(dunya_view PUBLIC
    dunya::core
    dunya::objectmodel
)

if (MSVC)
    target_compile_options(dunya_view PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_view PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_view Runtime)
