cmake_minimum_required(VERSION 3.20)

project(Shrinkigon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenMesh REQUIRED)
find_package(CURL REQUIRED)

add_executable(Shrinkigon MACOSX_BUNDLE
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

target_link_libraries(speedigon_cli PRIVATE
        assimp
        OpenMeshCore
        OpenMeshTools
)

target_compile_definitions(Shrinkigon PRIVATE GL_SILENCE_DEPRECATION)

target_include_directories(Shrinkigon PRIVATE
        external/imgui
        external/imgui/backends
        ${FREETYPE_INCLUDE_DIRS}
)

target_link_libraries(Shrinkigon PRIVATE
        OpenGL::GL
        glfw
        Freetype::Freetype
        assimp::assimp
        OpenMeshCore
        OpenMeshTools
        CURL::libcurl
)

if(APPLE)
    set_target_properties(Shrinkigon PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "Shrinkigon"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.shrinkigon.app"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_ICON_FILE "icon.icns"
    )

    set(SHRINKIGON_RESOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/CaacupeOne-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Geo-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/OFL.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns"
    )

    foreach(RESOURCE_FILE IN LISTS SHRINKIGON_RESOURCES)
        if(NOT EXISTS "${RESOURCE_FILE}")
            message(FATAL_ERROR "Missing Shrinkigon resource: ${RESOURCE_FILE}")
        endif()
    endforeach()

    set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/CaacupeOne-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Geo-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/OFL.txt"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/fonts"
    )

    set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )

    target_sources(Shrinkigon PRIVATE ${SHRINKIGON_RESOURCES})

    install(TARGETS Shrinkigon
            BUNDLE DESTINATION .
    )
endif()
