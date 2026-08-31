add_library(dunya_stb STATIC
    stb_impl.cc
)
add_library(dunya::stb ALIAS dunya_stb)
target_include_directories(dunya_stb PUBLIC "${stb_SOURCE_DIR}")
target_compile_features(dunya_stb PUBLIC cxx_std_23)

dunya_module(dunya_stb Runtime)
