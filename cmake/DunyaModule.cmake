set(DUNYA_MODULE_CATEGORIES Runtime Developer Editor)

set(DUNYA_MAY_LINK_Runtime   Runtime)
set(DUNYA_MAY_LINK_Developer Runtime Developer)
set(DUNYA_MAY_LINK_Editor    Runtime Developer Editor)

define_property(TARGET PROPERTY DUNYA_MODULE_CATEGORY
    BRIEF_DOCS "Module category"
    FULL_DOCS "Runtime, Developer or Editor"
)

set_property(GLOBAL PROPERTY DUNYA_DECLARED_MODULES "")

function(dunya_module target category)
    if (NOT category IN_LIST DUNYA_MODULE_CATEGORIES)
        message(FATAL_ERROR
            "${target} declares category '${category}'. "
            "It must be one of: ${DUNYA_MODULE_CATEGORIES}")
    endif()

    set_target_properties(${target} PROPERTIES DUNYA_MODULE_CATEGORY ${category})

    get_property(declared GLOBAL PROPERTY DUNYA_DECLARED_MODULES)
    list(APPEND declared ${target})
    set_property(GLOBAL PROPERTY DUNYA_DECLARED_MODULES "${declared}")
endfunction()

function(dunya_collect_targets directory out)
    get_property(here DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)

    set(found ${here})

    foreach(child ${children})
        dunya_collect_targets("${child}" nested)
        list(APPEND found ${nested})
    endforeach()

    set(${out} "${found}" PARENT_SCOPE)
endfunction()

function(dunya_resolve link out)
    get_target_property(aliased ${link} ALIASED_TARGET)

    if (aliased)
        set(${out} ${aliased} PARENT_SCOPE)
    else()
        set(${out} ${link} PARENT_SCOPE)
    endif()
endfunction()

function(dunya_check_modules)
    get_property(declared GLOBAL PROPERTY DUNYA_DECLARED_MODULES)

    set(violations "")

    foreach(target ${declared})
        get_target_property(category ${target} DUNYA_MODULE_CATEGORY)
        get_target_property(links ${target} LINK_LIBRARIES)

        if (NOT links)
            continue()
        endif()

        foreach(link ${links})
            if (NOT TARGET ${link})
                continue()
            endif()

            dunya_resolve(${link} resolved)

            get_target_property(linked ${resolved} DUNYA_MODULE_CATEGORY)

            if (NOT linked)
                continue()
            endif()

            if (NOT linked IN_LIST DUNYA_MAY_LINK_${category})
                list(APPEND violations
                    "  ${category} target '${target}' links ${linked} target '${resolved}'")
            endif()
        endforeach()
    endforeach()

    if (violations)
        list(JOIN violations "\n" report)
        message(FATAL_ERROR
            "Dunya module rule violated:\n${report}\n"
            "Runtime links Runtime. "
            "Developer links Runtime and Developer. "
            "Editor links anything.")
    endif()

    list(LENGTH declared count)

    message(STATUS "Dunya module rule: ${count} modules declared, no violations")
endfunction()

function(dunya_require_declared)
    get_property(declared GLOBAL PROPERTY DUNYA_DECLARED_MODULES)

    dunya_collect_targets("${PROJECT_SOURCE_DIR}" all)

    set(missing "")

    foreach(target ${all})
        get_target_property(type ${target} TYPE)

        if (type STREQUAL "UTILITY" OR type STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()

        get_target_property(imported ${target} IMPORTED)

        if (imported)
            continue()
        endif()

        get_target_property(source ${target} SOURCE_DIR)

        string(FIND "${source}" "${PROJECT_SOURCE_DIR}/build" inBuild)
        string(FIND "${source}" "_deps" inDeps)

        if (NOT inDeps EQUAL -1 OR NOT inBuild EQUAL -1)
            continue()
        endif()

        if (NOT target IN_LIST declared)
            list(APPEND missing "  ${target}  (${source})")
        endif()
    endforeach()

    if (missing)
        list(JOIN missing "\n" report)
        message(FATAL_ERROR
            "These targets declare no module category:\n${report}\n"
            "Call dunya_module(<target> <Runtime|Developer|Editor>).")
    endif()
endfunction()
