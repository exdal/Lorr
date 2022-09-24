add_subdirectory(Core)

add_library(Lorr INTERFACE)
    target_link_libraries(Lorr INTERFACE LCore)
    target_include_directories(Lorr INTERFACE ../)