#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

#include "frangi_processor.h"
#include "gui_manager.h"
#include "mask_filters.h"
#include <iostream>

// Глобальные параметры
struct AppState {
    // Параметры Frangi фильтра
    float sigma = 1.5f;
    float beta = 0.5f;
    float c = 15.0f;
    int displayStage = 6;  // 0-6
    bool invertEnabled = true;
    
    // Параметры препроцессинга
    bool globalContrastEnabled = false;
    float globalBrightness = 20.0f;
    float globalContrast = 3.0f;
    
    bool claheEnabled = false;
    int claheMaxIterations = 2;
    float claheTargetContrast = 0.3f;
    
    // Камера
    cv::VideoCapture camera;
    cv::Mat rawFrame;
    cv::Mat preprocessedFrame;  // После препроцессинга
    cv::Mat processedFrame;      // После Frangi
    
    // Frangi процессор
    FrangiProcessor* processor = nullptr;
    
    // FPS
    double lastTime = 0.0;
    int frameCount = 0;
    float fps = 0.0f;
    
    // Статус
    bool cameraActive = false;
    
    // ImGui текстуры для отображения видео
    GLuint rawFrameTexture = 0;
    GLuint processedFrameTexture = 0;
};

void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool initializeGLFW(GLFWwindow** window) {
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    *window = glfwCreateWindow(1600, 900, "Frangi Filter - Camera App", nullptr, nullptr);
    if (!*window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(*window);
    glfwSwapInterval(1); // Enable vsync
    
    return true;
}

bool initializeGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    return true;
}


bool initializeCamera(AppState& state) {
    state.camera.open(0);
    
    if (!state.camera.isOpened()) {
        std::cerr << "Failed to open camera" << std::endl;
        return false;
    }
    
    // Настройки камеры для минимальной задержки
    state.camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    state.camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    state.camera.set(cv::CAP_PROP_FPS, 30);
    
    std::cout << "Camera initialized: " 
              << state.camera.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << state.camera.get(cv::CAP_PROP_FRAME_HEIGHT) 
              << " @ " << state.camera.get(cv::CAP_PROP_FPS) << " FPS" << std::endl;
    
    state.cameraActive = true;
    return true;
}

void updateFPS(AppState& state) {
    double currentTime = glfwGetTime();
    state.frameCount++;
    
    if (currentTime - state.lastTime >= 1.0) {
        state.fps = state.frameCount / (currentTime - state.lastTime);
        state.frameCount = 0;
        state.lastTime = currentTime;
    }
}

void processFrame(AppState& state, MaskFilters& filters) {
    if (!state.cameraActive || !state.camera.isOpened()) {
        return;
    }
    
    // Захват кадра
    state.camera >> state.rawFrame;
    
    if (state.rawFrame.empty()) {
        std::cerr << "Failed to capture frame" << std::endl;
        return;
    }
    
    // Конвертация в grayscale для препроцессинга
    cv::Mat grayFrame;
    if (state.rawFrame.channels() == 3) {
        cv::cvtColor(state.rawFrame, grayFrame, cv::COLOR_BGR2GRAY);
    } else {
        grayFrame = state.rawFrame.clone();
    }
    
    // Применяем препроцессинг
    state.preprocessedFrame = grayFrame.clone();
    
    if (state.globalContrastEnabled) {
        state.preprocessedFrame = filters.applyGlobalContrast(
            state.preprocessedFrame, 
            state.globalBrightness, 
            state.globalContrast
        );
    }
    
    if (state.claheEnabled) {
        state.preprocessedFrame = filters.applyClahe(
            state.preprocessedFrame, 
            state.claheMaxIterations, 
            state.claheTargetContrast
        );
    }
    
    // Конвертируем обратно в BGR для процессора (если нужно)
    cv::Mat frameForProcessing;
    if (state.preprocessedFrame.channels() == 1) {
        cv::cvtColor(state.preprocessedFrame, frameForProcessing, cv::COLOR_GRAY2BGR);
    } else {
        frameForProcessing = state.preprocessedFrame;
    }
    
    // Обработка через Frangi
    if (state.processor) {
        state.processedFrame = state.processor->process(
            frameForProcessing, 
            state.sigma, 
            state.beta, 
            state.c,
            state.displayStage,
            state.invertEnabled
        );
    }
}


void cleanup(AppState& state, GLFWwindow* window) {
    // Освобождаем ресурсы
    if (state.processor) {
        delete state.processor;
    }
    
    if (state.camera.isOpened()) {
        state.camera.release();
    }
    
    // Удаляем ImGui текстуры
    if (state.rawFrameTexture) {
        glDeleteTextures(1, &state.rawFrameTexture);
    }
    if (state.processedFrameTexture) {
        glDeleteTextures(1, &state.processedFrameTexture);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    std::cout << "=== Frangi Filter Camera Application ===" << std::endl;
    std::cout << "Initializing..." << std::endl;
    
    AppState state;
    GLFWwindow* window = nullptr;
    GUIManager guiManager;
    MaskFilters maskFilters;
    
    // Инициализация GLFW
    if (!initializeGLFW(&window)) {
        return -1;
    }
    
    // Инициализация GLAD
    if (!initializeGLAD()) {
        glfwTerminate();
        return -1;
    }
    
    // Инициализация GUI Manager
    guiManager.initialize(window);
    
    // Инициализация Frangi процессора
    state.processor = new FrangiProcessor();
    if (!state.processor->initialize()) {
        std::cerr << "Failed to initialize Frangi processor" << std::endl;
        guiManager.shutdown();
        cleanup(state, window);
        return -1;
    }
    
    // Инициализация камеры
    if (!initializeCamera(state)) {
        std::cerr << "Failed to initialize camera" << std::endl;
        guiManager.shutdown();
        cleanup(state, window);
        return -1;
    }
    
    std::cout << "Initialization complete!" << std::endl;
    std::cout << "Using " << state.processor->getMethodName() << " for processing" << std::endl;
    
    state.lastTime = glfwGetTime();
    
    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Обработка кадра с камеры
        processFrame(state, maskFilters);
        
        // Обновление FPS
        updateFPS(state);
        
        // Рендеринг GUI через ImGui
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        guiManager.render(state);
        glfwSwapBuffers(window);
    }
    
    std::cout << "Shutting down..." << std::endl;
    guiManager.shutdown();
    cleanup(state, window);
    
    std::cout << "Application terminated successfully" << std::endl;
    return 0;
}
