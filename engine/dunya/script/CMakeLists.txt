file(GLOB DUNYA_NETHOST_CANDIDATES
    "$ENV{ProgramFiles}/dotnet/packs/Microsoft.NETCore.App.Host.win-x64/*/runtimes/win-x64/native"
)

if (NOT DUNYA_NETHOST_CANDIDATES)
    message(FATAL_ERROR
        "The .NET hosting pack was not found. Install the .NET 10 SDK, or point "
        "DUNYA_NETHOST_DIR at a directory holding nethost.h and libnethost.lib."
    )
endif()

list(SORT DUNYA_NETHOST_CANDIDATES)
list(GET DUNYA_NETHOST_CANDIDATES -1 DUNYA_NETHOST_DIR)

message(STATUS "dunya::script hosting pack: ${DUNYA_NETHOST_DIR}")

add_library(dunya_script STATIC
    api/api.cc
    host/host.cc
    runner/runner.cc
)

add_library(dunya::script ALIAS dunya_script)
target_include_directories(dunya_script PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_include_directories(dunya_script PRIVATE "${DUNYA_NETHOST_DIR}")
target_compile_features(dunya_script PUBLIC cxx_std_23)


target_link_libraries(dunya_script PUBLIC
    dunya::core
    dunya::objectmodel
    dunya::systems
)

target_link_libraries(dunya_script PRIVATE
    "${DUNYA_NETHOST_DIR}/nethost.lib"
)

if (MSVC)
    target_compile_options(dunya_script PRIVATE /W4 /WX /w15038 /w14062 /Zc:__cplusplus)
else()
    target_compile_options(dunya_script PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

find_program(DUNYA_DOTNET dotnet REQUIRED)

set(DUNYA_MANAGED_DIR "${CMAKE_BINARY_DIR}/managed")

add_custom_target(DUNYA_MANAGED ALL
    COMMAND "${DUNYA_DOTNET}" build
            "${PROJECT_SOURCE_DIR}/engine/managed/Dunya.Engine/Dunya.Engine.csproj"
            -c $<IF:$<CONFIG:Release>,Release,Debug>
            -o "${DUNYA_MANAGED_DIR}"
            --nologo -v quiet
    BYPRODUCTS "${DUNYA_MANAGED_DIR}/Dunya.Engine.dll"
    COMMENT "Building the managed engine assembly"
)

file(GLOB DUNYA_SCRIPT_PROJECTS "${PROJECT_SOURCE_DIR}/projects/*/scripts/Scripts.csproj")

foreach(DUNYA_SCRIPT_PROJECT ${DUNYA_SCRIPT_PROJECTS})
    get_filename_component(DUNYA_SCRIPT_DIR "${DUNYA_SCRIPT_PROJECT}" DIRECTORY)
    get_filename_component(DUNYA_PROJECT_DIR "${DUNYA_SCRIPT_DIR}" DIRECTORY)
    get_filename_component(DUNYA_PROJECT_NAME "${DUNYA_PROJECT_DIR}" NAME)

    add_custom_target(DUNYA_SCRIPTS_${DUNYA_PROJECT_NAME} ALL
        COMMAND "${DUNYA_DOTNET}" build "${DUNYA_SCRIPT_PROJECT}"
                -c $<IF:$<CONFIG:Release>,Release,Debug>
                -o "${CMAKE_BINARY_DIR}/projects/${DUNYA_PROJECT_NAME}/scripts"
                --nologo -v quiet
        DEPENDS DUNYA_MANAGED
        COMMENT "Building scripts for project ${DUNYA_PROJECT_NAME}"
    )
endforeach()

file(COPY "${DUNYA_NETHOST_DIR}/nethost.dll" DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY "${DUNYA_NETHOST_DIR}/nethost.dll" DESTINATION "${CMAKE_BINARY_DIR}/tests")

dunya_module(dunya_script Runtime)
