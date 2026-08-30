add_library(dunya_serialize INTERFACE)
add_library(dunya::serialize ALIAS dunya_serialize)
target_include_directories(dunya_serialize INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(dunya_serialize INTERFACE cxx_std_23)
target_link_libraries(dunya_serialize INTERFACE
    dunya::core
    dunya::field
    dunya::objectmodel
    glaze::glaze
)
