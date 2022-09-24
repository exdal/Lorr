add_library(tracy STATIC TracyClient.cpp)
    target_include_directories(tracy PUBLIC .)
