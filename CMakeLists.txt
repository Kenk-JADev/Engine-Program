cmake_minimum_required(VERSION 3.16)
project(RPGMaker3D VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RPGMAKER3D_ENABLE_RUBY "Enable mruby scripting support" OFF)
option(RPGMAKER3D_BUILD_EDITOR "Build integrated editor" ON)

# Lokale Dependencies (kann durch systemweite Installation oder vcpkg ersetzt werden)
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
    src/Camera.cpp
    src/Framebuffer.cpp
    src/Logger.cpp
    src/AudioPreview.cpp
    src/Command.cpp
    src/CommandHistory.cpp
    src/Prefab.cpp
    src/Raycast.cpp
    src/Lighting.cpp
    src/Material.cpp
    src/ParticleSystem.cpp
    ${GLAD_ROOT}/src/gl.c
    third_party/stb/stb_image_impl.c
    third_party/miniaudio/miniaudio_impl.c
)

if(RPGMAKER3D_BUILD_EDITOR)
    list(APPEND SOURCES
        src/Editor.cpp
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_demo.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_sdl2.cpp
        third_party/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

list(APPEND SOURCES src/RubyVM.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    third_party
    third_party/imgui
    third_party/imgui/backends
    ${GLAD_ROOT}/include
    ${GLM_ROOT}
)

# SDL2 finden (vcpkg, System oder lokaler Build)
set(SDL2_HINTS "")
if(EXISTS "${SDL2_ROOT}")
    list(APPEND SDL2_HINTS "${SDL2_ROOT}")
endif()

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Fallback: manuell suchen
    find_path(SDL2_INCLUDE_DIR SDL.h
        HINTS ${SDL2_HINTS}
        PATH_SUFFIXES include include/SDL2 SDL2
    )
    find_library(SDL2_LIBRARY SDL2
        HINTS ${SDL2_HINTS}
        PATH_SUFFIXES lib
    )
    find_library(SDL2MAIN_LIBRARY SDL2main
        HINTS ${SDL2_HINTS}
        PATH_SUFFIXES lib
    )
    if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
        message(FATAL_ERROR "SDL2 not found. Set SDL2_ROOT to the SDL2 install directory.")
    endif()
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
    if(SDL2MAIN_LIBRARY)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})

# OpenGL
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    OpenGL::GL
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)
endif()

# Windows-spezifisch
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm imm32 version setupapi)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        X11 Xext Xi Xfixes Xcursor Xinerama Xrandr Xss
        dl pthread rt m
    )
endif()

if(RPGMAKER3D_ENABLE_RUBY)
    set(MRUBY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mruby" CACHE PATH "mruby root")
    target_include_directories(${PROJECT_NAME} PRIVATE ${MRUBY_ROOT}/include)

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${MRUBY_ROOT}/build/host/lib/libmruby.lib
            ${MRUBY_ROOT}/build/host/lib/libmruby_core.lib
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${MRUBY_ROOT}/build/host/lib/libmruby.a
            ${MRUBY_ROOT}/build/host/lib/libmruby_core.a
        )
    endif()
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
