add_library(dunya_package STATIC
    package.cc
)
add_library(dunya::package ALIAS dunya_package)
target_include_directories(dunya_package PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_package PUBLIC cxx_std_23)
if (MSVC)
    target_compile_options(dunya_package PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_package PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_package Editor)
