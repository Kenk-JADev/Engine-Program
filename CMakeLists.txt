cmake_minimum_required(VERSION 3.16)
project(RPGMaker3D VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RPGMAKER3D_ENABLE_RUBY "Enable mruby scripting support" OFF)
option(RPGMAKER3D_BUILD_EDITOR "Build integrated editor" ON)

# Lokale Dependencies (kann durch systemweite Installation ersetzt werden)
set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/install" CACHE PATH "SDL2 install root")
set(GLAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad_generated" CACHE PATH "glad generated root")
set(GLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm" CACHE PATH "glm root")

# Quellen
set(SOURCES
    src/main.cpp
    src/Engine.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Model.cpp
    src/AudioManager.cpp
    src/Input.cpp
    src/Scene.cpp
    src/Map.cpp
    src/Tileset.cpp
    src/Project.cpp
    src/ResourceManager.cpp
    ${GLAD_ROOT}/src/gl.c
    third_party/stb/stb_image_impl.c
    third_party/miniaudio/miniaudio_impl.c
)

if(RPGMAKER3D_BUILD_EDITOR)
    list(APPEND SOURCES
        src/Editor.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_sdl2.cpp
        third_party/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

if(RPGMAKER3D_ENABLE_RUBY)
    list(APPEND SOURCES src/RubyVM.cpp)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    third_party
    third_party/imgui
    third_party/imgui/backends
    ${GLAD_ROOT}/include
    ${GLM_ROOT}
    ${SDL2_ROOT}/include
)

# SDL2 Linken
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SDL2_ROOT}/lib/SDL2.lib
        ${SDL2_ROOT}/lib/SDL2main.lib
        opengl32
    )
else()
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SDL2_ROOT}/lib/libSDL2.a
        ${SDL2_ROOT}/lib/libSDL2main.a
        GL
        X11
        Xext
        Xi
        Xfixes
        Xcursor
        Xinerama
        Xrandr
        Xss
        dl
        pthread
        rt
        m
    )
endif()

if(RPGMAKER3D_ENABLE_RUBY)
    set(MRUBY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mruby" CACHE PATH "mruby root")
    target_include_directories(${PROJECT_NAME} PRIVATE ${MRUBY_ROOT}/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${MRUBY_ROOT}/build/host/lib/libmruby.a
        ${MRUBY_ROOT}/build/host/lib/libmruby_core.a
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE RPGMAKER3D_ENABLE_RUBY)
endif()

if(RPGMAKER3D_BUILD_EDITOR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RPGMAKER3D_BUILD_EDITOR)
endif()

# Assets kopieren
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Windows-spezifisch
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
