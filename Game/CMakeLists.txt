set(GAME_VENDOR
    Lorr
)

set(GAME_COMPONENTS
    
)

add_executable(Game main.cc)
    target_link_libraries(Game PUBLIC ${GAME_VENDOR} ${GAME_COMPONENTS})
    target_include_directories(Game PUBLIC .)
    set_target_properties(Game PROPERTIES OUTPUT_NAME "Game-${CMAKE_BUILD_TYPE}")
    
