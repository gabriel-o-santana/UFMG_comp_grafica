cmake_minimum_required(VERSION 3.10)
project(boids-simulacao VERSION 1.0.0)

add_subdirectory(libs/glfw)

include_directories(libs/glad)
include_directories(libs/KHR)
include_directories(libs/imgui)
include_directories(libs/glfw/include/GLFW)
include_directories(libs)

include_directories(libs/glfw/include)
include_directories(include)

set(SOURCES
    src/main.cpp
    src/glad.cpp
    src/utils/utility.cpp
    src/shaders/shader.cpp
    src/display/base_window.cpp
    src/display/game_window.cpp
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
)
add_executable(boids-simulacao ${SOURCES})

find_package(OpenGL REQUIRED)

target_link_libraries(boids-simulacao PRIVATE ${OPENGL_gl_LIBRARY})
target_link_libraries(boids-simulacao PRIVATE glfw gdi32 user32 shell32)

file(COPY resources DESTINATION ${CMAKE_BINARY_DIR})