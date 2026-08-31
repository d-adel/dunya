add_library(dunya_platform STATIC
    glfwlibrary/glfwlibrary.cc
    input/input.cc
    window/window.cc
)
add_library(dunya::platform ALIAS dunya_platform)
target_include_directories(dunya_platform PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_compile_features(dunya_platform PUBLIC cxx_std_23)
target_link_libraries(dunya_platform PUBLIC
    dunya::core
    glfw
)
if (MSVC)
    target_compile_options(dunya_platform PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_platform PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_platform Runtime)
