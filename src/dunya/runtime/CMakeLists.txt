add_library(dunya_runtime STATIC
    deformation/deformation.cc
    runtime/runtime.cc
)
add_library(dunya::runtime ALIAS dunya_runtime)
target_include_directories(dunya_runtime PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_runtime PUBLIC cxx_std_20)

target_link_libraries(dunya_runtime PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
    dunya::physics
)
if (MSVC)
    target_compile_options(dunya_runtime PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
