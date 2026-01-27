find_package(SDL2 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(phase_02_player_movement
    main.cpp
    tilemap.hpp
    wave/player.hpp
)

target_include_directories(phase_02_player_movement
    PRIVATE
        third_party
        src/third_party
)

target_link_libraries(phase_02_player_movement
    PRIVATE
        SDL2::SDL2
        ZLIB::ZLIB
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(phase_02_player_movement PRIVATE SDL2::SDL2main)
endif()
