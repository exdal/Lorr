function(LorrComponent)

cmake_parse_arguments(
    COMPONENT
        "" # options
        "NAME;LIBTYPE" # one value
        "INCLUDE;SOURCE;VENDOR;PCH" # multi value
        ${ARGN}
    )

    if (NOT COMPONENT_NAME)
        message(FATAL_ERROR "Component name has to be set!")
    endif()

    if (NOT COMPONENT_SOURCE)
        message(FATAL_ERROR "You cannot create a component without source code.")
    endif()

    file(GLOB_RECURSE SOURCES
        ${COMPONENT_SOURCE}
    )

    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        add_library(${COMPONENT_NAME} ${COMPONENT_LIBTYPE} ${SOURCES})
    else()
        add_library(${COMPONENT_NAME} STATIC ${SOURCES})
    endif()

    if (COMPONENT_PCH)
        target_precompile_headers(${COMPONENT_NAME} PUBLIC ${COMPONENT_PCH})
    endif()

    if(COMPONENT_VENDOR)
        target_link_libraries(${COMPONENT_NAME} PUBLIC ${COMPONENT_VENDOR})
    endif()
    
    target_include_directories(${COMPONENT_NAME} PUBLIC ${COMPONENT_INCLUDE})
    target_compile_definitions(${COMPONENT_NAME} PUBLIC "LR_BUILD_SHARED=$<CONFIG:Debug>")

endfunction(LorrComponent)