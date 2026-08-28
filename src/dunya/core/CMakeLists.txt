# Foundation: the numbers both toolchains share, the id the object model
# indexes by, and a dispatcher that is plain C++. Nothing of ours sits below
# it, so it links nothing of ours either.

add_library(dunya_core STATIC
    event/event.cc
)

add_library(dunya::core ALIAS dunya_core)

target_include_directories(dunya_core PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_core PUBLIC cxx_std_20)

# PUBLIC because config.h is a header consumers include and these are the
# values it reads. The shaders are compiled with the same ones; see the parent
# CMakeLists, which is the only place that reaches both compilers. Anything
# linking dunya::core inherits them, which is why no other target declares
# them - idiom 27.
target_compile_definitions(dunya_core PUBLIC
    DUNYA_MAX_FIELD_PRIMITIVES=${DUNYA_MAX_FIELD_PRIMITIVES}
    DUNYA_MAX_TEXTURES=${DUNYA_MAX_TEXTURES}
    DUNYA_MAX_SAMPLERS=${DUNYA_MAX_SAMPLERS}
    DUNYA_MAX_MATERIALS=${DUNYA_MAX_MATERIALS}
    DUNYA_MAX_FIELD_RECORDS=${DUNYA_MAX_FIELD_RECORDS}
    DUNYA_MAX_FIELD_VOLUMES=${DUNYA_MAX_FIELD_VOLUMES}
    DUNYA_MAX_PRIMITIVE_POOL=${DUNYA_MAX_PRIMITIVE_POOL}
    DUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}
    DUNYA_SAMPLER_LINEAR_REPEAT=${DUNYA_SAMPLER_LINEAR_REPEAT}
    DUNYA_SAMPLER_LINEAR_CLAMP=${DUNYA_SAMPLER_LINEAR_CLAMP}
    DUNYA_SAMPLER_NEAREST_CLAMP=${DUNYA_SAMPLER_NEAREST_CLAMP}
)

if (MSVC)
    target_compile_options(dunya_core PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
