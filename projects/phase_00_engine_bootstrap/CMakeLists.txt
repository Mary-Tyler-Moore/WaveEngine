find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(phase_00_engine_bootstrap
    main.cpp
)

target_link_libraries(phase_00_engine_bootstrap
    PRIVATE
        SDL2::SDL2
        SDL2_image::SDL2_image
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(phase_00_engine_bootstrap PRIVATE SDL2::SDL2main)
endif()
