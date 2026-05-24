CXX = g++
CC = gcc
TARGET = main.exe
SRC = src/main.cpp src/game/Game.cpp src/game/City.cpp src/engine/Mesh.cpp src/engine/Model.cpp src/engine/Texture.cpp
LOADER = src/glad_loader.c
LOADER_OBJ = src/glad_loader.o

INCLUDES = -Isrc/engine -IDependencies/include -IDependencies/include/SOIL2 -IDependencies/include/glm
LIBDIRS = -LDependencies/lib
LIBS = -lassimp -lsoil2 -lglfw3 -lglew32s -lopengl32 -lgdi32 -luser32
CXXFLAGS = -std=c++11 -O3 -Wall $(INCLUDES)
CFLAGS = -O3 -Wall $(INCLUDES)

all: copy_libs $(TARGET)

copy_libs:
	@if not exist Dependencies\lib mkdir Dependencies\lib
	@if exist SOIL2-master\build\libsoil2.a copy SOIL2-master\build\libsoil2.a Dependencies\lib\libsoil2.a >nul 2>&1 || echo WARNING: Could not copy SOIL2 library

$(LOADER_OBJ): $(LOADER)
	$(CC) $(CFLAGS) -c $(LOADER) -o $(LOADER_OBJ)

$(TARGET): $(SRC) $(LOADER_OBJ)
	$(CXX) $(CXXFLAGS) $(SRC) $(LOADER_OBJ) -o $(TARGET) $(LIBDIRS) $(LIBS)
	@if exist C:\msys64\ucrt64\bin\libassimp-6.dll copy C:\msys64\ucrt64\bin\libassimp-6.dll libassimp-6.dll >nul 2>&1

clean:
	del /Q $(TARGET) src\glad_loader.o 2>NUL || exit 0
