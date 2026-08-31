add_library(dunya_imagecompare STATIC
    imagecompare.cc
)
add_library(dunya::imagecompare ALIAS dunya_imagecompare)
target_include_directories(dunya_imagecompare PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_link_libraries(dunya_imagecompare PRIVATE dunya::stb)
target_compile_features(dunya_imagecompare PUBLIC cxx_std_23)
if (MSVC)
    target_compile_options(dunya_imagecompare PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_imagecompare PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_imagecompare Developer)
