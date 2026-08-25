FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.7.1
  SYSTEM
)

FetchContent_MakeAvailable(Catch2)

add_executable(field_tests
    commandhistory_tests.cc
    field_tests.cc
    fieldobject_tests.cc
    raycast_tests.cc
    sampled_tests.cc

    # The object model is not a library, so these are compiled in directly.
    # All four are Vulkan-free, which keeps the boundary above honest.
    ${PROJECT_SOURCE_DIR}/src/command/command.cc
    ${PROJECT_SOURCE_DIR}/src/commandhistory/commandhistory.cc
    ${PROJECT_SOURCE_DIR}/src/fieldobject/fieldobject.cc
    ${PROJECT_SOURCE_DIR}/src/objectregistry/objectregistry.cc
)

target_include_directories(field_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(field_tests PRIVATE dunya::field Catch2::Catch2WithMain)

# config.h reads these. The numbers themselves are still written once, at the
# top of the parent CMakeLists.
target_compile_definitions(field_tests PRIVATE
    DUNYA_MAX_FIELD_PRIMITIVES=${DUNYA_MAX_FIELD_PRIMITIVES}
    DUNYA_MAX_TEXTURES=${DUNYA_MAX_TEXTURES}
    DUNYA_MAX_SAMPLERS=${DUNYA_MAX_SAMPLERS}
    DUNYA_MAX_MATERIALS=${DUNYA_MAX_MATERIALS}
    DUNYA_MAX_FIELD_OBJECTS=${DUNYA_MAX_FIELD_OBJECTS}
    DUNYA_SAMPLER_LINEAR_REPEAT=${DUNYA_SAMPLER_LINEAR_REPEAT}
    DUNYA_SAMPLER_LINEAR_CLAMP=${DUNYA_SAMPLER_LINEAR_CLAMP}
    DUNYA_SAMPLER_NEAREST_CLAMP=${DUNYA_SAMPLER_NEAREST_CLAMP}
)

if (MSVC)
    target_compile_options(field_tests PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(field_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Its own target rather than more files in field_tests: the two link different
# libraries, and a target named for the field should not be the place image
# comparison lives.
add_executable(image_tests
    image_tests.cc
)

target_link_libraries(image_tests PRIVATE dunya::imagecompare Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(image_tests PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(image_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

catch_discover_tests(field_tests)
catch_discover_tests(image_tests)

# The golden image tests are add_test rather than Catch2 cases because they run
# the renderer itself: they need a device, a window and the assets that get
# copied beside the executable, none of which a unit test should be dragging in.
#
# Both representations are covered. They are different code paths through the
# same march, and M27 is about to move one of them.
set(GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/golden")

function(add_golden_test name)
    add_test(
        NAME "golden ${name}"
        COMMAND ${PROJECT_NAME} --golden "${GOLDEN_DIR}/${name}.png" ${ARGN}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endfunction()

add_golden_test(field-sampled --carves 6)
add_golden_test(field-analytic --carves 6 --analytic)
