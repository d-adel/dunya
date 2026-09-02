FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.7.1
  SYSTEM
)

FetchContent_MakeAvailable(Catch2)

add_executable(unit_tests
    asset_tests.cc
    assetdatabase_tests.cc
    boundedextent_tests.cc
    recordbounds_tests.cc
    maincamera_tests.cc
    capi_tests.cc
    commandhistory_tests.cc
    crack_tests.cc
    deform_tests.cc
    dynamiccomponents_tests.cc
    entt_tests.cc
    input_tests.cc
    ../engine/capi/win32keys/win32keys.cc
    field_tests.cc
    glmmeta_tests.cc
    worldmeta_tests.cc
    materialfile_tests.cc
    worldfile_tests.cc
    viewport_tests.cc
    viewportstore_tests.cc
    lookthrough_tests.cc
    camerapose_tests.cc
    fieldshape_tests.cc
    impact_tests.cc
    lens_tests.cc
    componentcoverage_tests.cc
    instantiateworld_tests.cc
    pose_tests.cc
    physics_tests.cc
    project_tests.cc
    rangestore_tests.cc
    raycast_tests.cc
    redistance_tests.cc
    runtime_tests.cc
    schedule_tests.cc
    screenray_tests.cc
    scriptverbs_tests.cc
    scriptbridge_tests.cc
    scripthost_tests.cc
    sdfprimitivestore_tests.cc
    telemetry_tests.cc
    sampled_tests.cc
    worldcontents_tests.cc
    componentjson_tests.cc
    world_tests.cc
)

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(unit_tests PRIVATE dunya::view dunya::script dunya::core dunya::editor dunya::field dunya::objectmodel dunya::physics dunya::runtime dunya::serialize dunya::systems Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(unit_tests PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(spirv_reflect STATIC
    "$ENV{VULKAN_SDK}/Source/SPIRV-Reflect/spirv_reflect.c"
)
target_include_directories(spirv_reflect SYSTEM PUBLIC "$ENV{VULKAN_SDK}/Source/SPIRV-Reflect")
dunya_module(spirv_reflect Developer)

add_executable(spirv_tests
    spirv_tests.cc
)
target_include_directories(spirv_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(spirv_tests PRIVATE dunya::gizmos spirv_reflect dunya::renderer dunya::view dunya::field dunya::gpu Catch2::Catch2WithMain)
target_compile_definitions(spirv_tests PRIVATE DUNYA_SPIRV_DIR="${CMAKE_BINARY_DIR}/shaders")
add_dependencies(spirv_tests SHADER_TARGET)
if (MSVC)
    target_compile_options(spirv_tests PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(spirv_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
dunya_module(spirv_tests Editor)

add_executable(image_tests
    image_tests.cc
)
target_link_libraries(image_tests PRIVATE dunya::imagecompare Catch2::Catch2WithMain)
if (MSVC)
    target_compile_options(image_tests PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(image_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

catch_discover_tests(unit_tests)
catch_discover_tests(image_tests)
catch_discover_tests(spirv_tests)

set(GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/golden")
set(GOLDEN_GAME_DIR "${CMAKE_BINARY_DIR}/builds/demo")

add_test(
    NAME "package demo"
    COMMAND DunyaPackager
            $<TARGET_FILE:DunyaPlayer>
            "${CMAKE_BINARY_DIR}/projects/demo"
            "${GOLDEN_GAME_DIR}"
            main
            golden
            wall-big
)
set_tests_properties("package demo" PROPERTIES FIXTURES_SETUP packaged_demo)

function(add_golden_test name)
    add_test(
        NAME "golden ${name}"
        COMMAND "${GOLDEN_GAME_DIR}/demo.exe"
                --golden "${GOLDEN_DIR}/${name}.png" ${ARGN}
        WORKING_DIRECTORY "${GOLDEN_GAME_DIR}"
    )
    set_tests_properties(
        "golden ${name}" PROPERTIES FIXTURES_REQUIRED packaged_demo
    )
endfunction()
add_golden_test(field-sampled --world golden)
add_golden_test(field-analytic --world golden --analytic)

add_dependencies(unit_tests DUNYA_MANAGED DUNYA_SCRIPTS_puzzle)
target_compile_definitions(unit_tests PRIVATE DUNYA_CAPI_DIR="${PROJECT_SOURCE_DIR}/engine/capi" DUNYA_MANAGED_DIR="${CMAKE_BINARY_DIR}/managed" DUNYA_SCRIPTS_DIR="${CMAKE_BINARY_DIR}/projects/puzzle/scripts")

dunya_module(unit_tests Editor)
dunya_module(image_tests Developer)
