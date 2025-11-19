#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

#include "frangi_processor.h"
#include "gui_manager.h"
#include "mask_filters.h"
#include "settings.h"
#include <iostream>
#include <vector>
#include <string>

// Информация о камере
struct CameraInfo {
    int index;
    std::string name;
    int width;
    int height;
    double fps;
    bool available;
    
    CameraInfo() : index(-1), width(0), height(0), fps(0), available(false) {}
};

// Глобальные параметры
struct AppState {
    // Параметры Frangi фильтра (загружаются из settings)
    float sigma;
    float beta;
    float c;
    int displayStage;
    bool invertEnabled;
    float segmentationThreshold;
    
    // Параметры препроцессинга (загружаются из settings)
    bool globalContrastEnabled;
    float globalBrightness;
    float globalContrast;
    
    bool claheEnabled;
    int claheMaxIterations;
    float claheTargetContrast;
    
    // Камера
    cv::VideoCapture camera;
    cv::Mat rawFrame;
    cv::Mat preprocessedFrame;  // После препроцессинга
    cv::Mat processedFrame;      // После Frangi
    
    // Список доступных камер
    std::vector<CameraInfo> availableCameras;
    int selectedCameraIndex = 0;  // Индекс в availableCameras, не индекс камеры!
    
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
    
    // Настройки
    AppSettings settings;
};

// Загрузить настройки в AppState
void loadSettingsToState(AppState& state) {
    state.sigma = state.settings.sigma;
    state.beta = state.settings.beta;
    state.c = state.settings.c;
    state.displayStage = state.settings.displayStage;
    state.invertEnabled = state.settings.invertEnabled;
    state.segmentationThreshold = state.settings.segmentationThreshold;
    
    state.globalContrastEnabled = state.settings.globalContrastEnabled;
    state.globalBrightness = state.settings.globalBrightness;
    state.globalContrast = state.settings.globalContrast;
    
    state.claheEnabled = state.settings.claheEnabled;
    state.claheMaxIterations = state.settings.claheMaxIterations;
    state.claheTargetContrast = state.settings.claheTargetContrast;
    
    state.selectedCameraIndex = state.settings.selectedCameraIndex;
}

// Сохранить текущие значения в настройки
void saveStateToSettings(AppState& state) {
    state.settings.sigma = state.sigma;
    state.settings.beta = state.beta;
    state.settings.c = state.c;
    state.settings.displayStage = state.displayStage;
    state.settings.invertEnabled = state.invertEnabled;
    state.settings.segmentationThreshold = state.segmentationThreshold;
    
    state.settings.globalContrastEnabled = state.globalContrastEnabled;
    state.settings.globalBrightness = state.globalBrightness;
    state.settings.globalContrast = state.globalContrast;
    
    state.settings.claheEnabled = state.claheEnabled;
    state.settings.claheMaxIterations = state.claheMaxIterations;
    state.settings.claheTargetContrast = state.claheTargetContrast;
    
    state.settings.selectedCameraIndex = state.selectedCameraIndex;
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


// Сканирование доступных камер
void scanAvailableCameras(AppState& state) {
    state.availableCameras.clear();
    std::cout << "Scanning for available cameras..." << std::endl;
    
    // Проверяем первые 10 индексов камер
    for (int i = 0; i < 10; i++) {
        cv::VideoCapture testCap(i);
        
        if (testCap.isOpened()) {
            CameraInfo info;
            info.index = i;
            info.available = true;
            
            // Получаем параметры камеры
            info.width = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_WIDTH));
            info.height = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_HEIGHT));
            info.fps = testCap.get(cv::CAP_PROP_FPS);
            
            // Пробуем захватить кадр для проверки
            cv::Mat testFrame;
            testCap >> testFrame;
            
            if (!testFrame.empty()) {
                // Формируем имя камеры
                info.name = "Camera " + std::to_string(i) + " (" + 
                           std::to_string(info.width) + "x" + 
                           std::to_string(info.height) + ")";
                
                state.availableCameras.push_back(info);
                
                std::cout << "  ✓ Found: " << info.name 
                          << " @ " << info.fps << " FPS" << std::endl;
            }
            
            testCap.release();
        }
    }
    
    if (state.availableCameras.empty()) {
        std::cerr << "No cameras found!" << std::endl;
    } else {
        std::cout << "Found " << state.availableCameras.size() << " camera(s)" << std::endl;
    }
}

bool initializeCamera(AppState& state) {
    // Сканируем камеры если список пустой
    if (state.availableCameras.empty()) {
        scanAvailableCameras(state);
    }
    
    // Если камер нет, выходим
    if (state.availableCameras.empty()) {
        std::cerr << "No cameras available" << std::endl;
        return false;
    }
    
    // Открываем выбранную камеру
    int cameraIndex = state.availableCameras[state.selectedCameraIndex].index;
    std::cout << "Opening camera index: " << cameraIndex << std::endl;
    
    state.camera.open(cameraIndex);
    
    if (!state.camera.isOpened()) {
        std::cerr << "Failed to open camera " << cameraIndex << std::endl;
        return false;
    }
    
    // Настройки камеры для минимальной задержки
    state.camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    state.camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    state.camera.set(cv::CAP_PROP_FPS, 30);
    
    std::cout << "Camera " << cameraIndex << " initialized: " 
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
    
    // Обработка через Frangi (препроцессинг применяется внутри processor)
    if (state.processor) {
        state.processedFrame = state.processor->process(
            state.rawFrame, 
            state.sigma, 
            state.beta, 
            state.c,
            state.displayStage,
            state.invertEnabled,
            state.globalContrastEnabled,
            state.globalBrightness,
            state.globalContrast,
            state.claheEnabled,
            state.claheMaxIterations,
            state.claheTargetContrast,
            state.segmentationThreshold
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
    
    // Загрузка настроек
    std::cout << "Loading settings..." << std::endl;
    SettingsManager::loadSettings("settings.json", state.settings);
    loadSettingsToState(state);
    
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
