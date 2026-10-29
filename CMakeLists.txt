cmake_minimum_required(VERSION 3.20)

project(Shrinkigon)

set(CMAKE_CXX_STANDARD 20)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    include_directories("/opt/homebrew/include")
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenMesh REQUIRED)

find_package(CURL REQUIRED)

add_executable(Shrinkigon
        main.cpp
        optimization_algorithms.cpp
        localization.cpp
        app_settings.cpp
        cloud_client.cpp
        theme.cpp
        scene_renderer.cpp
        settings_ui.cpp
        mesh_codec.cpp
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        imgui_opengl21_renderer.h
        imgui_opengl21_renderer.cpp
)

add_executable(speedigon_cli
        main_cli.cpp
        optimization_algorithms.cpp
)

target_link_libraries(speedigon_cli
        assimp
        OpenMeshCore
        OpenMeshTools
)

target_compile_definitions(Shrinkigon PRIVATE GL_SILENCE_DEPRECATION)

target_link_libraries(Shrinkigon PRIVATE CURL::libcurl)

if(APPLE)
    set_target_properties(Shrinkigon PROPERTIES
            MACOSX_BUNDLE TRUE
    )
endif()

target_include_directories(Shrinkigon PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
        ${FREETYPE_INCLUDE_DIRS}
        /opt/homebrew/include
)

target_link_libraries(Shrinkigon PRIVATE
        OpenGL::GL
        glfw
        Freetype::Freetype
        assimp::assimp
        OpenMeshCore
        OpenMeshTools
)