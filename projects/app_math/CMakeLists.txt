find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(app_math main.cpp)
target_link_libraries(app_math
    PRIVATE
      SDL2::SDL2
      SDL2::SDL2main   # harmless on Linux, required on Windows
      imgui::imgui)
