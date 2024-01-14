function(add_example name)
cmake_parse_arguments(
    ARG
        "" # options
        "PATH" # one value
        "VENDOR" # multi value
        ${ARGN}
    )

    file(GLOB_RECURSE SOURCES
        ${ARG_PATH}
    )

    add_executable(${name} ${SOURCES})
        target_link_libraries(${name} PUBLIC Lorr ${ARG_VENDOR})
        target_include_directories(${name} PUBLIC .)
        set_target_properties(${name} PROPERTIES OUTPUT_NAME "${name}-${CMAKE_BUILD_TYPE}")
endfunction()

add_example(BareBones PATH BareBones/*.cc)