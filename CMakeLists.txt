cmake_minimum_required(VERSION 3.10)
project(opengl-imgui-cmake-template VERSION 0.1.0)

# Adiciona a pasta do GLFW como um sub-projeto (isso compila a biblioteca)
add_subdirectory(libs/glfw)

include_directories(libs/glad)
include_directories(libs/KHR)
include_directories(libs/imgui)
include_directories(libs/glfw/include/GLFW)
include_directories(libs)

# Aponta para onde os headers do GLFW estão no código fonte
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
add_executable(opengl-imgui-cmake-template ${SOURCES})

find_package(OpenGL REQUIRED)

# Linkamos com as libs do sistema e com o target 'glfw' (criado pelo add_subdirectory)
target_link_libraries(opengl-imgui-cmake-template PRIVATE ${OPENGL_gl_LIBRARY})
target_link_libraries(opengl-imgui-cmake-template PRIVATE glfw gdi32 user32 shell32)

# Copia resources para a pasta de build onde o executável está
file(COPY resources DESTINATION ${CMAKE_BINARY_DIR})