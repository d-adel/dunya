add_library(dunya_field STATIC
    analytic/analytic.cc
    deform/deform.cc
    raycast/raycast.cc
    redistance/redistance.cc
    sampled/sampled.cc
    primitive.cc
)
add_library(dunya::field ALIAS dunya_field)

target_include_directories(dunya_field PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(dunya_field PUBLIC glm::glm)

target_compile_features(dunya_field PUBLIC cxx_std_20)
target_compile_definitions(dunya_field PUBLIC
    GLM_FORCE_RADIANS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    DUNYA_MARCH_EPSILON=${DUNYA_MARCH_EPSILON}
    DUNYA_MARCH_MAX_ITERATIONS=${DUNYA_MARCH_MAX_ITERATIONS}
    DUNYA_MARCH_MAX_DISTANCE=${DUNYA_MARCH_MAX_DISTANCE}
    DUNYA_GRADIENT_EPSILON=${DUNYA_GRADIENT_EPSILON}
    DUNYA_MARCH_OMEGA=${DUNYA_MARCH_OMEGA}
    DUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}
)
if (MSVC)
    target_compile_options(dunya_field PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(dunya_field PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
