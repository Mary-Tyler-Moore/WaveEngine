find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(app_hello main.cpp)
target_link_libraries(app_hello
    PRIVATE
      SDL2::SDL2
      SDL2::SDL2main   # required on Windows
      imgui::imgui)
