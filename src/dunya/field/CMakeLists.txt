# The field evaluator is deliberately free of Vulkan and windowing: the test
# target links it on its own, which is what keeps that boundary honest.

add_library(dunya_field STATIC
    analytic/analytic.cc
    raycast/raycast.cc
    sampled/sampled.cc
    primitive.cc
)

add_library(dunya::field ALIAS dunya_field)

target_include_directories(dunya_field PUBLIC "${PROJECT_SOURCE_DIR}/src")

# PUBLIC, not PRIVATE: Primitive uses glm types in a header consumers include.
target_link_libraries(dunya_field PUBLIC glm::glm)

target_compile_features(dunya_field PUBLIC cxx_std_20)

# These change glm's type layout, so the library and every consumer must agree
# or Primitive is a different struct on each side of the link.
target_compile_definitions(dunya_field PUBLIC
    GLM_FORCE_RADIANS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL

    # The shaders are compiled with these same values; see the parent
    # CMakeLists. PUBLIC because they appear in default arguments in headers.
    DUNYA_MARCH_EPSILON=${DUNYA_MARCH_EPSILON}
    DUNYA_MARCH_MAX_ITERATIONS=${DUNYA_MARCH_MAX_ITERATIONS}
    DUNYA_MARCH_MAX_DISTANCE=${DUNYA_MARCH_MAX_DISTANCE}
    DUNYA_GRADIENT_EPSILON=${DUNYA_GRADIENT_EPSILON}
    DUNYA_MARCH_OMEGA=${DUNYA_MARCH_OMEGA}
    DUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}
)

if (MSVC)
    target_compile_options(dunya_field PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_field PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
