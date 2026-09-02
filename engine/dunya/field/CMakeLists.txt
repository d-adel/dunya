add_library(dunya_field STATIC
    analytic/analytic.cc
    deform/deform.cc
    raycast/raycast.cc
    redistance/redistance.cc
    sampledsdf/sampledsdf.cc
    primitive.cc
)
add_library(dunya::field ALIAS dunya_field)

target_include_directories(dunya_field PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(dunya_field PUBLIC glm::glm)

target_compile_features(dunya_field PUBLIC cxx_std_23)
target_compile_definitions(dunya_field PUBLIC
    DUNYA_MARCH_EPSILON=${DUNYA_MARCH_EPSILON}
    DUNYA_MARCH_MAX_ITERATIONS=${DUNYA_MARCH_MAX_ITERATIONS}
    DUNYA_MARCH_MAX_DISTANCE=${DUNYA_MARCH_MAX_DISTANCE}
    DUNYA_GRADIENT_EPSILON=${DUNYA_GRADIENT_EPSILON}
    DUNYA_MARCH_OMEGA=${DUNYA_MARCH_OMEGA}
    DUNYA_SHADOW_MAX_DISTANCE=${DUNYA_SHADOW_MAX_DISTANCE}
    DUNYA_SHADOW_SHARPNESS=${DUNYA_SHADOW_SHARPNESS}
    DUNYA_BRICK_CELLS=${DUNYA_BRICK_CELLS}
    DUNYA_SHAPE_SPHERE=${DUNYA_SHAPE_SPHERE}
    DUNYA_SHAPE_BOX=${DUNYA_SHAPE_BOX}
    DUNYA_SHAPE_PLANE=${DUNYA_SHAPE_PLANE}
    DUNYA_SHAPE_CYLINDER=${DUNYA_SHAPE_CYLINDER}
    DUNYA_CSG_UNION=${DUNYA_CSG_UNION}
    DUNYA_CSG_SMOOTH_UNION=${DUNYA_CSG_SMOOTH_UNION}
    DUNYA_CSG_INTERSECTION=${DUNYA_CSG_INTERSECTION}
    DUNYA_CSG_SUBTRACTION=${DUNYA_CSG_SUBTRACTION}
    DUNYA_CSG_SMOOTH_SUBTRACTION=${DUNYA_CSG_SMOOTH_SUBTRACTION}
)
if (MSVC)
    target_compile_options(dunya_field PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_field PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

dunya_module(dunya_field Runtime)
