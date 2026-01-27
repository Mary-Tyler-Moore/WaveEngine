find_package(SDL2 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(phase_01_tilemap_rendering
    main.cpp
    tilemap.hpp
)

target_include_directories(phase_01_tilemap_rendering
    PRIVATE
        third_party
        src/third_party
)

target_link_libraries(phase_01_tilemap_rendering
    PRIVATE
        SDL2::SDL2
        ZLIB::ZLIB
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(phase_01_tilemap_rendering PRIVATE SDL2::SDL2main)
endif()
