find_package(SDL2 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(phase_05_collision_detection
    main.cpp
    src/wave/ChunkId.cpp
    src/wave/ChunkId.hpp
    src/wave/ChunkMap.cpp
    src/wave/ChunkMap.hpp
    src/wave/ChunkStore.cpp
    src/wave/ChunkStore.hpp
    src/wave/CollisionMap.cpp
    src/wave/CollisionMap.hpp
    src/wave/OverlayText.cpp
    src/wave/OverlayText.hpp
    src/wave/BitmapFont8x8.hpp
    src/wave/TelemetryCsv.cpp
    src/wave/TelemetryCsv.hpp
    src/wave/TilemapLoader.cpp
    src/wave/TilemapLoader.hpp
    wave/player.hpp
)

target_include_directories(phase_05_collision_detection
    PRIVATE
        third_party
        src
        src/third_party
)

target_link_libraries(phase_05_collision_detection
    PRIVATE
        SDL2::SDL2
        ZLIB::ZLIB
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(phase_05_collision_detection PRIVATE SDL2::SDL2main)
endif()
