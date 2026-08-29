FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.7.1
  SYSTEM
)

FetchContent_MakeAvailable(Catch2)

add_executable(unit_tests
    boundedextent_tests.cc
    commandhistory_tests.cc
    entt_tests.cc
    field_tests.cc
    fieldshape_tests.cc
    instantiateworld_tests.cc
    pose_tests.cc
    physics_tests.cc
    rangestore_tests.cc
    raycast_tests.cc
    sdfprimitivestore_tests.cc
    sampled_tests.cc
    world_tests.cc
)

target_include_directories(unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(unit_tests PRIVATE dunya::core dunya::editor dunya::field dunya::objectmodel dunya::physics dunya::runtime Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(unit_tests PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Its own target rather than more files in unit_tests: the two link different
# libraries, and image comparison has no business sharing a binary with the
# object model and the field.
add_executable(image_tests
    image_tests.cc
)

target_link_libraries(image_tests PRIVATE dunya::imagecompare Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(image_tests PRIVATE /W4 /WX /w15038 /Zc:__cplusplus)
else()
    target_compile_options(image_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

catch_discover_tests(unit_tests)
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
