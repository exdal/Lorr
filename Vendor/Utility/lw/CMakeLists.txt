file(GLOB source *.cpp)
file(GLOB source_c *.c)
add_library(lw STATIC
    ${source}
    ${source_c}
)

target_include_directories(lw PUBLIC .)