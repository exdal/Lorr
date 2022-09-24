file(GLOB_RECURSE source *.cpp)
add_library(imgui STATIC
    ${source}
)

target_include_directories(imgui PUBLIC .)