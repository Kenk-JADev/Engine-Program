cmake_minimum_required(VERSION 3.16)
project(RPGMaker3D VERSION 0.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optionen
option(RPGMAKER3D_ENABLE_RUBY "Enable mruby scripting support" OFF)
option(RPGMAKER3D_BUILD_EDITOR "Build editor host (Qt)" ON)
option(RPGMAKER3D_BUILD_PLAYER "Build standalone game player" ON)
option(RPGMAKER3D_ENABLE_CONSOLE "Keep console window on Windows (for debugging)" ON)
option(RPGMAKER3D_ENABLE_RMLUI "Enable RmlUi UI system" ON)
# Default ON: Qt ist der Editor-Host. CI installiert Qt via aqtinstall (siehe .github/workflows/Main.yml).
# Lokal ohne Qt: -DRPGMAKER3D_EDITOR_QT=OFF
option(RPGMAKER3D_EDITOR_QT "Build Qt-based editor (native windows, QOpenGLWidget + Code Workspace)" ON)
option(RPGMAKER3D_ENABLE_IMGUI "Legacy ImGui overlay for GameUI (off by default; ImGui editor removed)" OFF)

if(RPGMAKER3D_EDITOR_QT AND MSVC AND VCPKG_TARGET_TRIPLET MATCHES ".*-static")
    message(WARNING
        "RPGMAKER3D_EDITOR_QT: Qt-DLLs sind /MD, das '${VCPKG_TARGET_TRIPLET}'-Triplet"
        " liefert /MT-Static-Libs -> CRT-Mix. Bitte Triplet 'x64-windows' verwenden"
        " (siehe docs/QT-EDITOR.md).")
endif()

# ==================== RmlUi + FreeType (vendored, statisch) ====================
# Ziel: ImGui langfristig ersetzen. Alles aus third_party, keine neuen System-Deps.
# WICHTIG: MSVC-Runtime (/MT) wird vor den Subdirectories gesetzt, damit
# freetype/rmlui mit derselben CRT wie die Engine bauen.
if(RPGMAKER3D_ENABLE_RMLUI)
    if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES ".*-static")
        set(_RPG_PREV_CRT "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # CMake >= 4.0 entfernt Kompatibilitaet mit min-required < 3.5
    # (RmlUi deklariert 3.10). Auf 3.x-CMake harmlos ignoriert.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()

    set(_RPG_PREV_BSL "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)

    # FreeType: abgespeckt (keine optionalen Kompression/PNG/Harfbuzz-Deps)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    if(NOT TARGET freetype)
        add_subdirectory(third_party/freetype)
    endif()
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    # RmlUi: nur Core als statische Lib (kein Samples/Tests/Lua/Debugger)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
    add_subdirectory(third_party/rmlui)

    set(BUILD_SHARED_LIBS "${_RPG_PREV_BSL}")
    if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES ".*-static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "${_RPG_PREV_CRT}")
    endif()
endif()

message(STATUS "==== RPG Maker 3D Engine v${PROJECT_VERSION} ====")
message(STATUS "Editor: ${RPGMAKER3D_BUILD_EDITOR}, Qt: ${RPGMAKER3D_EDITOR_QT}, ImGui: ${RPGMAKER3D_ENABLE_IMGUI}, Ruby: ${RPGMAKER3D_ENABLE_RUBY}")

# Pfade zu lokalen Dependencies
set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/install" CACHE PATH "SDL2 install root")
set(GLAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad_generated" CACHE PATH "glad generated root")
set(GLM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm" CACHE PATH "glm root")

# Fallback für glad
if(NOT EXISTS "${GLAD_ROOT}/include/glad/gl.h")
    message(WARNING "glad not found at ${GLAD_ROOT}, trying third_party/glad2 fallback")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include/glad/glad.h")
        set(GLAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad" CACHE PATH "glad fallback" FORCE)
    endif()
endif()

# ==================== Quellen ====================

set(CORE_LIB_SOURCES
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
    src/Command.cpp
    src/CommandHistory.cpp
    src/Prefab.cpp
    src/Raycast.cpp
    src/Lighting.cpp
    src/ShadowMap.cpp
    src/Material.cpp
    src/ParticleSystem.cpp
    src/RubyVM.cpp
    src/ScriptManager.cpp
    src/Platform.cpp
    src/Database.cpp
    src/EventSystem.cpp
    src/Game.cpp
    src/BattleSystem.cpp
    src/UI.cpp
    src/Custom.cpp
    src/RgssUI.cpp
)

if(RPGMAKER3D_ENABLE_RMLUI)
    # WICHTIG: muss vor set(CORE_SOURCES ...) stehen (Listen sind Snapshots)
    list(APPEND CORE_LIB_SOURCES src/RmlUiSystem.cpp)
endif()

set(CORE_SOURCES
    src/main.cpp
    ${CORE_LIB_SOURCES}
)

set(THIRDPARTY_SOURCES
    ${GLAD_ROOT}/src/gl.c
    third_party/miniaudio/miniaudio_impl.c
)

if(RPGMAKER3D_ENABLE_RMLUI)
    list(APPEND THIRDPARTY_SOURCES third_party/rmlui_glue/RmlUiRenderGL3.cpp)
endif()

# ImGui-Editor ist entfernt. Optional nur noch GameUI-Overlay (legacy).
if(RPGMAKER3D_ENABLE_IMGUI)
    list(APPEND CORE_SOURCES
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_demo.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_sdl2.cpp
        third_party/imgui/backends/imgui_impl_opengl3.cpp
    )
    # AudioPreview war ImGui-Panel – nur mit ImGui bauen
    list(APPEND CORE_SOURCES src/AudioPreview.cpp)
    list(APPEND CORE_LIB_SOURCES src/AudioPreview.cpp)
endif()

if(RPGMAKER3D_EDITOR_QT)
    # Qt-Editor: nativer Host (ersetzt SDL-main + ImGui-Editor)
    if(NOT RPGMAKER3D_BUILD_EDITOR)
        message(FATAL_ERROR "RPGMAKER3D_EDITOR_QT benoetigt RPGMAKER3D_BUILD_EDITOR=ON")
    endif()
    # Soft-find: CI ohne installiertes Qt (oder Workflow noch ohne aqt) faellt
    # sauber auf SDL-main zurueck, statt Configure hart abzubrechen.
    find_package(Qt6 QUIET COMPONENTS Widgets OpenGLWidgets)
    if(Qt6_FOUND)
        message(STATUS "Qt-Editor: Qt ${Qt6_VERSION} (${Qt6_DIR})")
        list(REMOVE_ITEM CORE_SOURCES src/main.cpp)
        list(APPEND CORE_SOURCES
            qt_editor/QtMain.cpp
            qt_editor/QtEditorWindow.cpp
            qt_editor/QtGameViewWidget.cpp
            qt_editor/QtCodeWorkspace.cpp
            qt_editor/QtSyntaxHighlighter.cpp
            qt_editor/QtMapTab.cpp
            qt_editor/QtMapEditorDock.cpp
            qt_editor/QtDatabaseEditorDock.cpp
            qt_editor/QtDatabaseDialog.cpp
            qt_editor/QtSoundTestDialog.cpp
            qt_editor/QtMapPropertiesDialog.cpp
            qt_editor/QtEventEditorDock.cpp
            qt_editor/QtEventCommandCatalog.cpp
            qt_editor/QtEventCommandsDialog.cpp
            qt_editor/QtEventCommandEditDialog.cpp
            qt_editor/QtEventEditorDialog.cpp
            qt_editor/QtAssetBrowserDock.cpp
            # Q_OBJECT-Header explizit auflisten, damit AUTOMOC sie findet
            qt_editor/QtEditorWindow.h
            qt_editor/QtGameViewWidget.h
            qt_editor/QtCodeWorkspace.h
            qt_editor/QtSyntaxHighlighter.h
            qt_editor/QtMapTab.h
            qt_editor/QtMapEditorDock.h
            qt_editor/QtDatabaseEditorDock.h
            qt_editor/QtDatabaseDialog.h
            qt_editor/QtSoundTestDialog.h
            qt_editor/QtMapPropertiesDialog.h
            qt_editor/QtEventEditorDock.h
            qt_editor/QtEventCommandCatalog.h
            qt_editor/QtEventCommandsDialog.h
            qt_editor/QtEventCommandEditDialog.h
            qt_editor/QtEventEditorDialog.h
            qt_editor/QtAssetBrowserDock.h
        )
    else()
        message(WARNING
            "RPGMAKER3D_EDITOR_QT=ON, aber Qt6 wurde nicht gefunden. "
            "Fallback: SDL main.cpp ohne Qt-Editor. "
            "Installiere Qt 6 und setze CMAKE_PREFIX_PATH, oder siehe "
            "BUILD-WINDOWS-WORKFLOW.yml / docs/QT-EDITOR.md (aqtinstall).")
        set(RPGMAKER3D_EDITOR_QT OFF CACHE BOOL "Qt editor (auto-disabled: Qt6 not found)" FORCE)
    endif()
endif()
if(NOT RPGMAKER3D_EDITOR_QT AND RPGMAKER3D_BUILD_EDITOR)
    message(STATUS "Editor-Host: SDL main.cpp (kein Qt, kein ImGui-Editor)")
endif()

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${THIRDPARTY_SOURCES}
)

# ==================== Editor Executable ====================

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resource.rc")
    target_sources(${PROJECT_NAME} PRIVATE resources/resource.rc)
    message(STATUS "Including Windows resource file")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    third_party
    third_party/imgui
    third_party/imgui/backends
    ${GLAD_ROOT}/include
    ${GLM_ROOT}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    _CRT_SECURE_NO_WARNINGS
)

# MSVC: UTF-8 source/execution charset (avoids C4566 on icon unicode escapes)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
endif()

if(RPGMAKER3D_BUILD_EDITOR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        RPGMAKER3D_BUILD_EDITOR
    )
endif()

if(RPGMAKER3D_ENABLE_IMGUI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        RPGMAKER3D_ENABLE_IMGUI
    )
endif()

# ==================== SDL2 ====================

set(SDL2_HINTS "")

if(EXISTS "${SDL2_ROOT}")
    list(APPEND SDL2_HINTS "${SDL2_ROOT}")
endif()

if(DEFINED ENV{VCPKG_ROOT})
    list(APPEND SDL2_HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows-static")
    list(APPEND SDL2_HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()

find_package(SDL2 QUIET CONFIG)

if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

if(NOT SDL2_FOUND)
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
        message(STATUS "SDL2 not found via find_package, trying third_party/SDL2")

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/CMakeLists.txt")
            set(SDL_SHARED OFF CACHE BOOL "" FORCE)
            set(SDL_STATIC ON CACHE BOOL "" FORCE)
            set(SDL_TEST OFF CACHE BOOL "" FORCE)

            add_subdirectory(third_party/SDL2 EXCLUDE_FROM_ALL)

            set(SDL2_FOUND TRUE)
            add_library(SDL2::SDL2 ALIAS SDL2-static)

            if(TARGET SDL2main)
                add_library(SDL2::SDL2main ALIAS SDL2main)
            endif()
        else()
            message(FATAL_ERROR
                "SDL2 not found. Set SDL2_ROOT or install via vcpkg: "
                "vcpkg install sdl2:x64-windows-static"
            )
        endif()
    else()
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

        set(SDL2_FOUND TRUE)
    endif()
endif()

if(SDL2_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_INCLUDE_DIRS}
    )
endif()

# ==================== OpenGL ====================

find_package(OpenGL REQUIRED)

# ==================== Editor Linking ====================

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
)

if(RPGMAKER3D_ENABLE_RMLUI)
    target_link_libraries(${PROJECT_NAME} PRIVATE rmlui_core)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RPGMAKER3D_ENABLE_RMLUI)
endif()

if(RPGMAKER3D_EDITOR_QT)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::OpenGLWidgets)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RPGMAKER3D_EDITOR_QT)
    set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
    endif()
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
elseif(TARGET SDL2-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2-static)
endif()

if(TARGET SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)
elseif(TARGET SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2main)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        winmm
        imm32
        version
        setupapi
        shlwapi
        shell32
        ole32
        oleaut32
        uuid
        advapi32
        ws2_32
        iphlpapi
    )

    if(NOT RPGMAKER3D_ENABLE_CONSOLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()

    set_property(
        TARGET ${PROJECT_NAME}
        PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if(VCPKG_TARGET_TRIPLET MATCHES ".*-static")
        if(MSVC)
            set_property(
                TARGET ${PROJECT_NAME}
                PROPERTY MSVC_RUNTIME_LIBRARY
                "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            )
        endif()
    endif()

    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE
            "/MANIFEST:NO"
        )
    endif()
else()
    find_package(Threads REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CMAKE_DL_LIBS}
        Threads::Threads
        m
    )

    find_package(X11 QUIET)

    if(X11_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${X11_LIBRARIES}
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            X11
            Xext
            Xi
            Xfixes
            Xcursor
            Xinerama
            Xrandr
            Xss
        )
    endif()
endif()

# ==================== mruby ====================

if(RPGMAKER3D_ENABLE_RUBY)
    set(
        MRUBY_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mruby"
        CACHE PATH
        "mruby root"
    )

    set(MRUBY_GENERATED_INCLUDE_DIR
        "${MRUBY_ROOT}/build/host/include"
    )

    if(NOT EXISTS "${MRUBY_ROOT}/include/mruby.h")
        message(FATAL_ERROR
            "mruby headers not found: ${MRUBY_ROOT}/include/mruby.h. "
            "Clone and build mruby first, or set MRUBY_ROOT."
        )
    endif()

    if(NOT EXISTS "${MRUBY_GENERATED_INCLUDE_DIR}/mruby/presym/id.h")
        message(FATAL_ERROR
            "mruby generated headers not found: "
            "${MRUBY_GENERATED_INCLUDE_DIR}/mruby/presym/id.h. "
            "Run ruby minirake in ${MRUBY_ROOT} first."
        )
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        "${MRUBY_GENERATED_INCLUDE_DIR}"
        "${MRUBY_ROOT}/include"
    )

    if(WIN32)
        set(MRUBY_CANDIDATES
            "${MRUBY_ROOT}/build/host/lib/libmruby.lib"
            "${MRUBY_ROOT}/build/host/lib/mruby.lib"
            "${MRUBY_ROOT}/build/host/lib/libmruby_core.lib"
            "${MRUBY_ROOT}/build/host/lib/mruby_core.lib"
        )

        foreach(lib_path IN LISTS MRUBY_CANDIDATES)
            if(EXISTS "${lib_path}")
                target_link_libraries(${PROJECT_NAME} PRIVATE
                    "${lib_path}"
                )

                message(STATUS "Linking mruby: ${lib_path}")
            endif()
        endforeach()

        if(
            NOT EXISTS "${MRUBY_ROOT}/build/host/lib/libmruby.lib"
            AND
            NOT EXISTS "${MRUBY_ROOT}/build/host/lib/mruby.lib"
        )
            message(FATAL_ERROR
                "mruby library not found. Run ruby minirake first."
            )
        endif()
    else()
        if(EXISTS "${MRUBY_ROOT}/build/host/lib/libmruby.a")
            target_link_libraries(${PROJECT_NAME} PRIVATE
                "${MRUBY_ROOT}/build/host/lib/libmruby.a"
            )
        endif()

        if(EXISTS "${MRUBY_ROOT}/build/host/lib/libmruby_core.a")
            target_link_libraries(${PROJECT_NAME} PRIVATE
                "${MRUBY_ROOT}/build/host/lib/libmruby_core.a"
            )
        endif()
    endif()

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        RPGMAKER3D_ENABLE_RUBY
    )
endif()

# ==================== Post Build: Editor Assets ====================

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to output directory"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/SampleProject
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/SampleProject
    COMMENT "Copying SampleProject"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ruby")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/ruby
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/ruby
        COMMENT "Copying ruby scripts"
    )
endif()

# ==================== Player Executable ====================

if(RPGMAKER3D_BUILD_PLAYER)
    set(PLAYER_SOURCES
        src/player_main.cpp
        ${CORE_LIB_SOURCES}
    )

    if(RPGMAKER3D_ENABLE_IMGUI)
        list(APPEND PLAYER_SOURCES
            third_party/imgui/imgui.cpp
            third_party/imgui/imgui_demo.cpp
            third_party/imgui/imgui_draw.cpp
            third_party/imgui/imgui_tables.cpp
            third_party/imgui/imgui_widgets.cpp
            third_party/imgui/backends/imgui_impl_sdl2.cpp
            third_party/imgui/backends/imgui_impl_opengl3.cpp
        )
    endif()

    list(APPEND PLAYER_SOURCES
        ${THIRDPARTY_SOURCES}
    )

    add_executable(${PROJECT_NAME}_Player
        ${PLAYER_SOURCES}
    )

    if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resource.rc")
        target_sources(${PROJECT_NAME}_Player PRIVATE
            resources/resource.rc
        )
    endif()

    target_include_directories(${PROJECT_NAME}_Player PRIVATE
        include
        third_party
        third_party/imgui
        third_party/imgui/backends
        ${GLAD_ROOT}/include
        ${GLM_ROOT}
    )

    target_compile_definitions(${PROJECT_NAME}_Player PRIVATE
        GLM_ENABLE_EXPERIMENTAL
        _CRT_SECURE_NO_WARNINGS
    )

    if(RPGMAKER3D_ENABLE_IMGUI)
        target_compile_definitions(${PROJECT_NAME}_Player PRIVATE RPGMAKER3D_ENABLE_IMGUI)
    endif()

    if(MSVC)
        target_compile_options(${PROJECT_NAME}_Player PRIVATE /utf-8)
    endif()

    if(WIN32)
        target_compile_definitions(${PROJECT_NAME}_Player PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            UNICODE
            _UNICODE
        )

        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            winmm
            imm32
            version
            setupapi
            shlwapi
            shell32
            ws2_32
            iphlpapi
        )

        if(NOT RPGMAKER3D_ENABLE_CONSOLE)
            set_target_properties(${PROJECT_NAME}_Player PROPERTIES
                WIN32_EXECUTABLE TRUE
            )
        endif()

        set_property(
            TARGET ${PROJECT_NAME}_Player
            PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        if(VCPKG_TARGET_TRIPLET MATCHES ".*-static")
            if(MSVC)
                set_property(
                    TARGET ${PROJECT_NAME}_Player
                    PROPERTY MSVC_RUNTIME_LIBRARY
                    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
                )
            endif()
        endif()

        if(MSVC)
            target_link_options(${PROJECT_NAME}_Player PRIVATE
                "/MANIFEST:NO"
            )
        endif()
    endif()

    if(TARGET SDL2::SDL2)
        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            SDL2::SDL2
        )
    elseif(TARGET SDL2-static)
        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            SDL2-static
        )
    endif()

    if(TARGET SDL2::SDL2main)
        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            SDL2::SDL2main
        )
    elseif(TARGET SDL2main)
        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            SDL2main
        )
    endif()

    target_link_libraries(${PROJECT_NAME}_Player PRIVATE
        OpenGL::GL
    )

    if(RPGMAKER3D_ENABLE_RMLUI)
        target_link_libraries(${PROJECT_NAME}_Player PRIVATE rmlui_core)
        target_compile_definitions(${PROJECT_NAME}_Player PRIVATE RPGMAKER3D_ENABLE_RMLUI)
    endif()

    if(NOT WIN32)
        find_package(Threads REQUIRED)

        target_link_libraries(${PROJECT_NAME}_Player PRIVATE
            ${CMAKE_DL_LIBS}
            Threads::Threads
            m
            X11
            Xext
        )
    endif()

    if(RPGMAKER3D_ENABLE_RUBY AND EXISTS "${MRUBY_ROOT}/include")
        target_include_directories(${PROJECT_NAME}_Player PRIVATE
            "${MRUBY_GENERATED_INCLUDE_DIR}"
            "${MRUBY_ROOT}/include"
        )

        set(PLAYER_MRUBY_CANDIDATES
            "${MRUBY_ROOT}/build/host/lib/libmruby.lib"
            "${MRUBY_ROOT}/build/host/lib/mruby.lib"
            "${MRUBY_ROOT}/build/host/lib/libmruby_core.lib"
            "${MRUBY_ROOT}/build/host/lib/mruby_core.lib"
            "${MRUBY_ROOT}/build/host/lib/libmruby.a"
        )

        foreach(lib_path IN LISTS PLAYER_MRUBY_CANDIDATES)
            if(EXISTS "${lib_path}")
                target_link_libraries(${PROJECT_NAME}_Player PRIVATE
                    "${lib_path}"
                )
            endif()
        endforeach()

        target_compile_definitions(${PROJECT_NAME}_Player PRIVATE
            RPGMAKER3D_ENABLE_RUBY
        )
    endif()

    add_custom_command(TARGET ${PROJECT_NAME}_Player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}_Player>/assets
        COMMENT "Copying player assets"
    )

    add_custom_command(TARGET ${PROJECT_NAME}_Player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/SampleProject
        $<TARGET_FILE_DIR:${PROJECT_NAME}_Player>/SampleProject
        COMMENT "Copying player sample project"
    )
endif()

# ==================== Installer / Package ====================

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(RPGMAKER3D_BUILD_PLAYER AND TARGET ${PROJECT_NAME}_Player)
    install(TARGETS ${PROJECT_NAME}_Player
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/assets
)

install(DIRECTORY SampleProject/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/SampleProject
    OPTIONAL
)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS" CACHE STRING "Packaging generators")
    set(CPACK_PACKAGE_NAME "RPGMaker3D")
    set(CPACK_PACKAGE_VENDOR "RPG Maker 3D Team")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RPG Maker 3D Engine - Windows")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_NSIS_DISPLAY_NAME "RPG Maker 3D Engine")
    set(CPACK_NSIS_PACKAGE_NAME "RPGMaker3D")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    include(CPack)
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Player: ${RPGMAKER3D_BUILD_PLAYER}")
