add_library(dunya_debugui STATIC
    panels/panels.cc
)

add_library(dunya::debugui ALIAS dunya_debugui)
target_include_directories(dunya_debugui PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_debugui PUBLIC cxx_std_23)

target_link_libraries(dunya_debugui PUBLIC
    dunya::core
)

if (MSVC)
    target_compile_options(dunya_debugui PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_debugui PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_debugui Developer)
