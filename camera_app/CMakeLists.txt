cmake_minimum_required(VERSION 3.16)

project(camera_app VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV (должен быть в C:\opencv\build)
set(OpenCV_DIR "C:/opencv/build" CACHE PATH "Path to OpenCV")
find_package(OpenCV REQUIRED)

# Find GLFW3
find_package(glfw3 CONFIG REQUIRED)

# Find ImGui
find_package(imgui CONFIG REQUIRED)

# Find GLAD
find_package(glad CONFIG REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find nlohmann_json for settings
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(camera_app
    main.cpp
    app_state.h
    settings/src/settings.cpp
    settings/headers/settings.h
    processors/src/frangi_processor.cpp
    processors/headers/frangi_processor.h
    processors/src/gl_renderer.cpp
    processors/headers/gl_renderer.h
    processors/src/frangi.cpp
    processors/headers/frangi.h
    processors/src/mask_filters.cpp
    processors/headers/mask_filters.h
    managers/src/window_manager.cpp
    managers/headers/window_manager.h
    managers/src/camera_manager.cpp
    managers/headers/camera_manager.h
    managers/src/gui_manager.cpp
    managers/headers/gui_manager.h
)

target_link_libraries(camera_app PRIVATE
    ${OpenCV_LIBS}
    glfw
    imgui::imgui
    glad::glad
    OpenGL::GL
    nlohmann_json::nlohmann_json
)

target_include_directories(camera_app PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/managers/headers
    ${CMAKE_CURRENT_SOURCE_DIR}/processors/headers
    ${CMAKE_CURRENT_SOURCE_DIR}/settings/headers
)

# Копирование settings.json в выходную директорию
add_custom_command(TARGET camera_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:camera_app>/settings/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/settings/configs/settings.json"
        "$<TARGET_FILE_DIR:camera_app>/settings/configs/settings.json"
    COMMENT "Copying settings.json to output directory"
)

# Windows specific settings
if(WIN32)
    set_target_properties(camera_app PROPERTIES
        WIN32_EXECUTABLE FALSE  # Keep console for debugging
    )
    
    # Автоматическое копирование OpenCV DLL
    if(EXISTS "C:/opencv/build/x64/vc16/bin")
        add_custom_command(TARGET camera_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/opencv/build/x64/vc16/bin/opencv_world4120.dll"
                $<TARGET_FILE_DIR:camera_app>
            COMMENT "Copying OpenCV DLL to output directory"
        )
        
        # Для Debug конфигурации
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_custom_command(TARGET camera_app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "C:/opencv/build/x64/vc16/bin/opencv_world4120d.dll"
                    $<TARGET_FILE_DIR:camera_app>
                COMMENT "Copying OpenCV Debug DLL to output directory"
            )
        endif()
    endif()
endif()