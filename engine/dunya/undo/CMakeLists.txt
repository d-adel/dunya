add_library(dunya_undo STATIC
    undostack/undostack.cc
)

add_library(dunya::undo ALIAS dunya_undo)
target_include_directories(dunya_undo PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_undo PUBLIC cxx_std_23)

target_link_libraries(dunya_undo PUBLIC
    dunya::objectmodel
)

if (MSVC)
    target_compile_options(dunya_undo PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_undo PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_undo Editor)
