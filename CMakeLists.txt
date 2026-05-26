cmake_minimum_required(VERSION 3.10)
project(MiniDeliveryDash)

# Encontrar paquetes instalados vía brew
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(assimp REQUIRED)

# Archivos fuente
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(MiniDeliveryDash ${SOURCES})

# Enlazar
target_link_libraries(MiniDeliveryDash glfw GLEW::GLEW assimp::assimp)

# Frameworks de Apple necesarios
if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(MiniDeliveryDash ${OPENGL_FRAMEWORK})
endif()