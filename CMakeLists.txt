cmake_minimum_required(VERSION 3.10)

project(MiniDeliveryDash LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

add_executable(MiniDeliveryDash
    ${SOURCES}
    third_party/glad/src/glad.c
)

target_include_directories(MiniDeliveryDash PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/third_party"
    "${CMAKE_SOURCE_DIR}/third_party/glad/include"
)

set_target_properties(MiniDeliveryDash PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(WIN32)

    target_compile_definitions(MiniDeliveryDash PRIVATE GLEW_STATIC)

    find_library(GLFW_LIBRARY
        NAMES glfw3 libglfw3
        PATHS "${CMAKE_SOURCE_DIR}/third_party/lib"
        NO_DEFAULT_PATH REQUIRED
    )

    find_library(ASSIMP_LIBRARY
        NAMES assimp libassimp
        PATHS "${CMAKE_SOURCE_DIR}/third_party/lib"
        NO_DEFAULT_PATH REQUIRED
    )

    find_library(SOIL2_LIBRARY
        NAMES soil2 libsoil2
        PATHS "${CMAKE_SOURCE_DIR}/third_party/lib"
        NO_DEFAULT_PATH REQUIRED
    )

    find_library(GLEW_LIBRARY
        NAMES glew32s
        PATHS "${CMAKE_SOURCE_DIR}/third_party/lib"
        NO_DEFAULT_PATH REQUIRED
    )

    target_link_libraries(MiniDeliveryDash PRIVATE
        ${GLFW_LIBRARY}
        ${ASSIMP_LIBRARY}
        ${SOIL2_LIBRARY}
        ${GLEW_LIBRARY}
        opengl32
        gdi32
        user32
    )

elseif(APPLE)

    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(assimp REQUIRED)

    target_link_libraries(MiniDeliveryDash PRIVATE
        glfw
        assimp::assimp
        OpenGL::GL
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )

else()

    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(assimp REQUIRED)

    target_link_libraries(MiniDeliveryDash PRIVATE
        glfw
        assimp::assimp
        OpenGL::GL
        dl
        pthread
    )

endif()