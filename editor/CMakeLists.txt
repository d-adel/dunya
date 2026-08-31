set(DUNYA_EDITOR_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/Dunya.Editor/Dunya.Editor.csproj")

set(DUNYA_EDITOR_NATIVE_DIR "$<TARGET_FILE_DIR:dunya_c>")

add_custom_target(DUNYA_EDITOR ALL
    COMMAND "${DUNYA_DOTNET}" build "${DUNYA_EDITOR_PROJECT}"
            -c $<IF:$<CONFIG:Release>,Release,Debug>
            -p:Platform=AnyCPU
            -p:DunyaNativeDir=${DUNYA_EDITOR_NATIVE_DIR}
            --nologo -v quiet
    DEPENDS dunya_c
    COMMENT "Building the editor against the freshly built dunya_c"
)
