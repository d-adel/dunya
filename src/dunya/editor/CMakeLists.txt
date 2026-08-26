# Tools for changing a world: what an edit is, and the history of the ones
# made so far. Vulkan-free on purpose - the unit tests link this, and a game
# build links none of it.
#
# The overlay is not here. It is ImGui and Vulkan, and pulling it in would
# drag both into every target that wants an undo stack.

add_library(dunya_editor STATIC
    command/command.cc
    commandhistory/commandhistory.cc
    fieldeditor/fieldeditor.cc
)

add_library(dunya::editor ALIAS dunya_editor)

target_include_directories(dunya_editor PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_compile_features(dunya_editor PUBLIC cxx_std_20)

target_link_libraries(dunya_editor PUBLIC
    dunya::core
    dunya::field
    dunya::objectmodel
)

if (MSVC)
    target_compile_options(dunya_editor PRIVATE /W4 /WX /Zc:__cplusplus)
else()
    target_compile_options(dunya_editor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
