cmake_minimum_required(VERSION 3.10)

project(MiniDeliveryDash)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =====================================
# Includes
# =====================================

include_directories(
    "${CMAKE_SOURCE_DIR}/Dependencies/include"
    "${CMAKE_SOURCE_DIR}/external"
    "${CMAKE_SOURCE_DIR}/external/glad/include"
    "${CMAKE_SOURCE_DIR}/src"
)

# =====================================
# Librerías
# =====================================

find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)

# =====================================
# Source files
# =====================================

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

# Glad oficial
set(GLAD_SOURCE
    external/glad/src/glad.c
)

# =====================================
# Executable
# =====================================

add_executable(MiniDeliveryDash
    ${SOURCES}
    ${GLAD_SOURCE}
)

# =====================================
# Link libraries
# =====================================

target_link_libraries(MiniDeliveryDash
    glfw
    assimp::assimp
)

# =====================================
# macOS
# =====================================

if(APPLE)

    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(MiniDeliveryDash ${OPENGL_FRAMEWORK})

    find_library(COCOA_FRAMEWORK Cocoa)
    target_link_libraries(MiniDeliveryDash ${COCOA_FRAMEWORK})

    find_library(IOKIT_FRAMEWORK IOKit)
    target_link_libraries(MiniDeliveryDash ${IOKIT_FRAMEWORK})

    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    target_link_libraries(MiniDeliveryDash ${COREVIDEO_FRAMEWORK})

endif() 