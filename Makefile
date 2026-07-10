CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude -Ithird_party -Ithird_party/imgui -Ithird_party/imgui/backends -Ithird_party/glad_generated/include -Ithird_party/glm -Ithird_party/SDL2/install/include/SDL2
CFLAGS = -O2 -Wall -Ithird_party -Ithird_party/glad_generated/include -Ithird_party/SDL2/install/include/SDL2
LDFLAGS = -Lthird_party/SDL2/install/lib -static-libstdc++ -static-libgcc
LDLIBS = third_party/SDL2/install/lib/libSDL2.a third_party/SDL2/install/lib/libSDL2main.a \
    /usr/lib/x86_64-linux-gnu/libGL.so.1 \
    /usr/lib/x86_64-linux-gnu/libX11.so \
    /usr/lib/x86_64-linux-gnu/libXext.so \
    /usr/lib/x86_64-linux-gnu/libXi.so.6 \
    /usr/lib/x86_64-linux-gnu/libXfixes.so.3 \
    /usr/lib/x86_64-linux-gnu/libXcursor.so.1 \
    /usr/lib/x86_64-linux-gnu/libXinerama.so.1 \
    /usr/lib/x86_64-linux-gnu/libXrandr.so.2 \
    /usr/lib/x86_64-linux-gnu/libXss.a \
    -ldl -lpthread -lrt -lm

SRC = src/main.cpp \
      src/Engine.cpp \
      src/Window.cpp \
      src/Renderer.cpp \
      src/Shader.cpp \
      src/Texture.cpp \
      src/Model.cpp \
      src/AudioManager.cpp \
      src/Input.cpp \
      src/Scene.cpp \
      src/Map.cpp \
      src/Tileset.cpp \
      src/Project.cpp \
      src/ResourceManager.cpp \
      src/Camera.cpp \
      src/Framebuffer.cpp \
      src/Logger.cpp \
      src/AudioPreview.cpp \
      src/Command.cpp \
      src/CommandHistory.cpp \
      src/Prefab.cpp \
      src/RubyVM.cpp \
      src/Raycast.cpp \
      src/Lighting.cpp \
      src/Material.cpp \
      src/ParticleSystem.cpp \
      src/Platform.cpp \
      src/Database.cpp \
      src/EventSystem.cpp \
      src/Game.cpp \
      src/BattleSystem.cpp \
      src/UI.cpp \
      src/Editor.cpp \
      third_party/imgui/imgui.cpp \
      third_party/imgui/imgui_demo.cpp \
      third_party/imgui/imgui_draw.cpp \
      third_party/imgui/imgui_tables.cpp \
      third_party/imgui/imgui_widgets.cpp \
      third_party/imgui/backends/imgui_impl_sdl2.cpp \
      third_party/imgui/backends/imgui_impl_opengl3.cpp \
      third_party/glad_generated/src/gl.c \
      third_party/stb/stb_image_impl.c \
      third_party/miniaudio/miniaudio_impl.c

OBJ = $(SRC:.cpp=.o)
OBJ := $(OBJ:.c=.o)

TARGET = rpgmaker3d

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Build complete: $@"
	@echo "Run with: ./$(TARGET) --editor"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Windows cross-build via mingw (optional)
win:
	@echo "Use scripts/build-windows.ps1 on Windows or GitHub Actions for Windows build"

help:
	@echo "RPG Maker 3D Engine Build"
	@echo "  make          - build linux executable"
	@echo "  make run      - build and run"
	@echo "  make clean    - clean build artifacts"
	@echo "Windows: use scripts/build-windows.ps1 or cmake"
