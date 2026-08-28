# The OS: a window, the library that opens it, and the keys and cursor it
# reports. Knows GLFW and nothing about Vulkan - Godot keeps platform/ apart
# from drivers/ for the same reason.

add_library(dunya_platform STATIC
    glfwlibrary/glfwlibrary.cc
    input/input.cc
    window/window.cc
)

add_library(dunya::platform ALIAS dunya_platform)

target_include_directories(dunya_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_platform PUBLIC cxx_std_20)

# PUBLIC: GLFW types and the event dispatcher both appear in headers
# consumers include.
target_link_libraries(dunya_platform PUBLIC
    dunya::core
    glfw
)

if (MSVC)
    target_compile_options(dunya_platform PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_platform PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
