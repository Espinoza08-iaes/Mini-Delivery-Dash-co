CXX = g++
CC = gcc
TARGET = main.exe
SRC = main.cpp Mesh.cpp Model.cpp Texture.cpp
LOADER = glad_loader.c
LOADER_OBJ = glad_loader.o

INCLUDES = -IDependencies/include -IDependencies/include/SOIL2 -IDependencies/include/glm
LIBDIRS = -LDependencies/lib
LIBS = -lsoil2 -lglfw3 -lglew32s -lopengl32 -lgdi32 -luser32
CXXFLAGS = -std=c++11 -O2 -Wall $(INCLUDES)
CFLAGS = -O2 -Wall $(INCLUDES)

all: copy_libs $(TARGET)

copy_libs:
	@if not exist Dependencies\lib mkdir Dependencies\lib
	@if exist SOIL2-master\build\libsoil2.a copy SOIL2-master\build\libsoil2.a Dependencies\lib\libsoil2.a >nul 2>&1 || echo WARNING: Could not copy SOIL2 library

$(LOADER_OBJ): $(LOADER)
	$(CC) $(CFLAGS) -c $(LOADER) -o $(LOADER_OBJ)

$(TARGET): $(SRC) Camera.h Shader.h Mesh.h Model.h Texture.h VAO.h VBO.h EBO.h Vertex.h $(LOADER_OBJ)
	$(CXX) $(CXXFLAGS) $(SRC) $(LOADER_OBJ) -o $(TARGET) $(LIBDIRS) $(LIBS)

clean:
	del /Q $(TARGET) $(LOADER_OBJ) 2>NUL || exit 0
