# Like the field library, this one is deliberately free of Vulkan and windowing:
# comparing two images needs no device, so the tests that decide what "the image
# changed" means can run without one.

add_library(dunya_imagecompare STATIC
    imagecompare.cc
    stb_impl.cc
)

add_library(dunya::imagecompare ALIAS dunya_imagecompare)

target_include_directories(dunya_imagecompare PUBLIC "${PROJECT_SOURCE_DIR}/src")

# PRIVATE: stb appears only in the implementation, never in the header, so
# consumers do not inherit it.
target_include_directories(dunya_imagecompare PRIVATE "${stb_SOURCE_DIR}")

target_compile_features(dunya_imagecompare PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options(dunya_imagecompare PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_imagecompare PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# stb's own warnings are silenced inside stb_impl.cc with pushed diagnostics
# rather than a per-source /W0 here, which MSVC reports as a collision with the
# target's /W4 (D9025) and so costs a warning to save one.
