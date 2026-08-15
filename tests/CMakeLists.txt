FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.7.1
  SYSTEM
)

FetchContent_MakeAvailable(Catch2)

add_executable(field_tests
    field_tests.cc
    raycast_tests.cc
    sampled_tests.cc
)

target_include_directories(field_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(field_tests PRIVATE dunya::field Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(field_tests PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(field_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

catch_discover_tests(field_tests)
