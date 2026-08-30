add_library(dunya_serialize STATIC
    editorstate/editorstate.cc
    project/project.cc
    textfile/textfile.cc
    materialfile/materialfile.cc
    worldfile/worldfile.cc
)
add_library(dunya::serialize ALIAS dunya_serialize)
target_include_directories(dunya_serialize PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_serialize PUBLIC cxx_std_23)
target_link_libraries(dunya_serialize PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
    glaze::glaze
)
if (MSVC)
    target_compile_options(dunya_serialize PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_serialize PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
