add_library(dunya_systems STATIC
    schedule/schedule.cc
)

add_library(dunya::systems ALIAS dunya_systems)
target_include_directories(dunya_systems PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_systems PUBLIC cxx_std_23)

target_link_libraries(dunya_systems PUBLIC
    dunya::objectmodel
)

if (MSVC)
    target_compile_options(dunya_systems PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_systems PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_systems Runtime)
