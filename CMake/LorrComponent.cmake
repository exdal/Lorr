function(LorrComponent)

cmake_parse_arguments(
    COMPONENT
        "" # options
        "NAME;LIBTYPE" # one value
        "INCLUDE_PRIV;INCLUDE_PUB;SOURCE;VENDOR;PCH" # multi value
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

    add_library(${COMPONENT_NAME} STATIC ${SOURCES})

    if (COMPONENT_PCH)
        target_precompile_headers(${COMPONENT_NAME} PUBLIC ${COMPONENT_PCH})
    endif()

    if(COMPONENT_VENDOR)
        target_link_libraries(${COMPONENT_NAME} PUBLIC ${COMPONENT_VENDOR})
    endif()
    
    if (COMPONENT_INCLUDE_PRIV)
        target_include_directories(${COMPONENT_NAME} PRIVATE ${COMPONENT_INCLUDE_PRIV})
    endif()
    
    if (COMPONENT_INCLUDE_PUB)
        target_include_directories(${COMPONENT_NAME} PUBLIC ${COMPONENT_INCLUDE_PUB})
    endif()
    
    target_compile_definitions(${COMPONENT_NAME} PUBLIC "LR_BUILD_SHARED=$<CONFIG:Debug>")

endfunction(LorrComponent)