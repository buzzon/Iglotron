#include "window_manager.h"
#include <iostream>

WindowManager::WindowManager() : window(nullptr), initialized(false) {
}

WindowManager::~WindowManager() {
    shutdown();
}

void WindowManager::errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool WindowManager::initialize(int width, int height, const std::string& title) {
    // Устанавливаем callback для ошибок
    glfwSetErrorCallback(errorCallback);
    
    // Инициализация GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // Настройки OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    // Создание окна
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    // Установка контекста OpenGL
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Инициализация GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        window = nullptr;
        return false;
    }
    
    // Вывод информации о OpenGL
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    initialized = true;
    return true;
}

void WindowManager::shutdown() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    
    if (initialized) {
        glfwTerminate();
        initialized = false;
    }
}

bool WindowManager::shouldClose() const {
    return window ? glfwWindowShouldClose(window) : true;
}

void WindowManager::swapBuffers() {
    if (window) {
        glfwSwapBuffers(window);
    }
}

void WindowManager::pollEvents() {
    if (window) {
        glfwPollEvents();
    }
}

std::string WindowManager::getOpenGLVersion() const {
    if (initialized) {
        return reinterpret_cast<const char*>(glGetString(GL_VERSION));
    }
    return "Not initialized";
}

std::string WindowManager::getGLSLVersion() const {
    if (initialized) {
        return reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    }
    return "Not initialized";
}

double WindowManager::getTime() const {
    return glfwGetTime();
}

