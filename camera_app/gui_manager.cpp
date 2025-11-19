#include "gui_manager.h"
#include "frangi_processor.h"
#include "settings.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
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

// Определение структуры AppState (должно совпадать с main.cpp)
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
    cv::Mat preprocessedFrame;
    cv::Mat processedFrame;
    
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
    
    // Аппрувинг (injection window)
    bool approvalEnabled;
    int approvalMaskHeight;
    int approvalMaskWidth;
    float approvalThreshold;
    float approvalRatio = 0.0f;
    
    // Настройки
    AppSettings settings;
};

// Forward declarations функций из main.cpp
extern void scanAvailableCameras(AppState& state);
extern void saveStateToSettings(AppState& state);
extern void loadSettingsToState(AppState& state);

GUIManager::GUIManager() : m_initialized(false) {
}

GUIManager::~GUIManager() {
    if (m_initialized) {
        shutdown();
    }
}

void GUIManager::initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Темная тема
    ImGui::StyleColorsDark();
    
    // Настройка стиля для лучшего вида
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 8);
    style.WindowRounding = 0.0f;
    
    // Инициализация backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    m_initialized = true;
    std::cout << "GUI Manager initialized" << std::endl;
}

void GUIManager::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    m_initialized = false;
    std::cout << "GUI Manager shutdown" << std::endl;
}

void GUIManager::updateTexture(GLuint* texture, const cv::Mat& image) {
    if (image.empty()) {
        return;
    }
    
    // Конвертируем в RGB если нужно
    cv::Mat rgb;
    if (image.channels() == 1) {
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgb, cv::COLOR_BGRA2RGB);
    } else {
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    }
    
    // Создаем текстуру если еще не создана
    if (*texture == 0) {
        glGenTextures(1, texture);
        glBindTexture(GL_TEXTURE_2D, *texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, *texture);
    }
    
    // Загружаем данные
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GUIManager::render(AppState& state) {
    if (!m_initialized) {
        return;
    }
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Единое полноэкранное окно
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::Begin("Main Window", nullptr, window_flags);
    
    // Разделяем окно на колонки: левая панель + видео
    ImGui::Columns(2, "MainColumns", false);
    ImGui::SetColumnWidth(0, 350);  // Ширина панели управления
    
    // Левая колонка: Панель управления
    renderControlPanel(state);
    
    // Правая колонка: Видеопотоки
    ImGui::NextColumn();
    renderVideoFeeds(state);
    
    ImGui::Columns(1);
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUIManager::renderControlPanel(AppState& state) {
    ImGui::BeginChild("ControlPanel", ImVec2(0, 0), true);
    
    ImGui::Text("FRANGI FILTER CAMERA");
    ImGui::Separator();
    
    // FPS и статус
    ImGui::Text("FPS: %.1f", state.fps);
    ImGui::Text("Camera: %s", state.cameraActive ? "Active" : "Inactive");
    ImGui::Text("Method: %s", state.processor ? state.processor->getMethodName() : "N/A");
    
    if (!state.rawFrame.empty()) {
        ImGui::Text("Resolution: %dx%d", state.rawFrame.cols, state.rawFrame.rows);
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Секция управления настройками
    if (ImGui::CollapsingHeader("Settings Management")) {
        ImGui::Indent();
        
        ImGui::Text("File: settings.json");
        
        if (ImGui::Button("Save Settings", ImVec2(-1, 0))) {
            saveStateToSettings(state);
            if (SettingsManager::saveSettings("settings.json", state.settings)) {
                std::cout << "✓ Settings saved successfully!" << std::endl;
            } else {
                std::cerr << "✗ Failed to save settings!" << std::endl;
            }
        }
        
        if (ImGui::Button("Load Settings", ImVec2(-1, 0))) {
            if (SettingsManager::loadSettings("settings.json", state.settings)) {
                loadSettingsToState(state);
                std::cout << "✓ Settings loaded successfully!" << std::endl;
            } else {
                std::cerr << "✗ Failed to load settings!" << std::endl;
            }
        }
        
        if (ImGui::Button("Reset to Defaults", ImVec2(-1, 0))) {
            if (SettingsManager::createDefaultSettings("settings.json")) {
                SettingsManager::loadSettings("settings.json", state.settings);
                loadSettingsToState(state);
                std::cout << "✓ Settings reset to defaults!" << std::endl;
            }
        }
        
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Секция выбора камеры
    if (ImGui::CollapsingHeader("Camera Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        if (!state.availableCameras.empty()) {
            // Создаем список имен камер для ComboBox
            std::vector<const char*> cameraNames;
            for (const auto& cam : state.availableCameras) {
                cameraNames.push_back(cam.name.c_str());
            }
            
            int previousSelection = state.selectedCameraIndex;
            if (ImGui::Combo("Camera", &state.selectedCameraIndex, cameraNames.data(), 
                            static_cast<int>(cameraNames.size()))) {
                // Камера изменилась, переподключаемся
                if (state.camera.isOpened()) {
                    std::cout << "Switching camera from index " 
                              << state.availableCameras[previousSelection].index 
                              << " to " << state.availableCameras[state.selectedCameraIndex].index 
                              << std::endl;
                    state.camera.release();
                    
                    int newCameraIndex = state.availableCameras[state.selectedCameraIndex].index;
                    state.camera.open(newCameraIndex);
                    
                    if (state.camera.isOpened()) {
                        // Применяем настройки
                        state.camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                        state.camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
                        state.camera.set(cv::CAP_PROP_FPS, 30);
                        state.cameraActive = true;
                        std::cout << "Camera switched successfully!" << std::endl;
                    } else {
                        std::cerr << "Failed to open camera " << newCameraIndex << std::endl;
                        state.cameraActive = false;
                    }
                }
            }
            
            // Информация о текущей камере
            if (state.selectedCameraIndex < state.availableCameras.size()) {
                const auto& selectedCam = state.availableCameras[state.selectedCameraIndex];
                ImGui::Text("Index: %d", selectedCam.index);
                ImGui::Text("Native: %dx%d @ %.1f FPS", 
                           selectedCam.width, selectedCam.height, selectedCam.fps);
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No cameras detected!");
        }
        
        // Кнопка для повторного сканирования
        if (ImGui::Button("Rescan Cameras", ImVec2(-1, 0))) {
            scanAvailableCameras(state);
        }
        
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Секция препроцессинга
    renderPreprocessingSection(state);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Секция Frangi
    renderFrangiSection(state);
    
    ImGui::EndChild();
}

void GUIManager::renderPreprocessingSection(AppState& state) {
    if (ImGui::CollapsingHeader("Preprocessing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        // Global Contrast
        ImGui::Checkbox("Global Contrast", &state.globalContrastEnabled);
        if (state.globalContrastEnabled) {
            ImGui::Indent();
            ImGui::SliderFloat("Brightness", &state.globalBrightness, 0.0f, 100.0f, "%.1f");
            ImGui::SliderFloat("Contrast", &state.globalContrast, 0.0f, 10.0f, "%.2f");
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        // CLAHE
        ImGui::Checkbox("CLAHE Enhancement", &state.claheEnabled);
        if (state.claheEnabled) {
            ImGui::Indent();
            ImGui::SliderInt("Max Iterations", &state.claheMaxIterations, 1, 5);
            ImGui::SliderFloat("Target Contrast", &state.claheTargetContrast, 0.0f, 0.9f, "%.2f");
            ImGui::Unindent();
        }
        
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Секция аппрувинга (Injection Window Approval)
    if (ImGui::CollapsingHeader("Injection Approval")) {
        ImGui::Indent();
        
        ImGui::Checkbox("Enable Approval Mask", &state.approvalEnabled);
        
        if (state.approvalEnabled) {
            ImGui::Spacing();
            ImGui::Text("Mask Size (pixels)");
            ImGui::SliderInt("Height##approval", &state.approvalMaskHeight, 50, 300);
            ImGui::SliderInt("Width##approval", &state.approvalMaskWidth, 50, 400);
            
            ImGui::Spacing();
            ImGui::Text("Approval Threshold");
            ImGui::SliderFloat("Min Vessel Ratio", &state.approvalThreshold, 0.0f, 1.0f, "%.2f");
            ImGui::Text("Current ratio: %.1f%%", state.approvalRatio * 100.0f);
            
            // Индикатор состояния
            ImGui::Spacing();
            if (state.approvalRatio >= state.approvalThreshold) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ APPROVED - Safe to inject");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ NOT APPROVED - Do not inject");
            }
            
            ImGui::Spacing();
            ImGui::TextWrapped("Mask position: center-bottom");
            ImGui::TextWrapped("Green = approved, Red = not approved");
        }
        
        ImGui::Unindent();
    }
}

void GUIManager::renderFrangiSection(AppState& state) {
    if (ImGui::CollapsingHeader("Frangi Filter", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        ImGui::SliderFloat("Sigma (Scale)", &state.sigma, 0.5f, 10.0f, "%.2f");
        ImGui::SliderFloat("Beta (Plate Sensitivity)", &state.beta, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("C (Contrast)", &state.c, 0.1f, 50.0f, "%.1f");
        
        ImGui::Spacing();
        
        const char* stages[] = {
            "0: Grayscale",
            "1: Invert",
            "2: Blur",
            "3: Gradients",
            "4: Hessian",
            "5: Eigenvalues",
            "6: Vesselness",
            "7: Segmentation",
            "8: Overlay"
        };
        ImGui::Combo("Display Stage", &state.displayStage, stages, 9);
        ImGui::Checkbox("Enable Invert (dark structures)", &state.invertEnabled);
        
        ImGui::Spacing();
        ImGui::Text("Segmentation");
        ImGui::SliderFloat("Threshold", &state.segmentationThreshold, 0.001f, 0.1f, "%.4f");
        
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Информация
    if (ImGui::CollapsingHeader("Information")) {
        ImGui::Indent();
        ImGui::TextWrapped("The Frangi filter detects vessel-like and line-like structures using Hessian matrix eigenvalue analysis.");
        ImGui::Spacing();
        ImGui::TextWrapped("Preprocessing enhances image quality before Frangi processing.");
        ImGui::Unindent();
    }
}

void GUIManager::renderVideoFeeds(AppState& state) {
    ImGui::BeginChild("VideoFeeds", ImVec2(0, 0), false);
    
    // Обновляем текстуры
    updateTexture(&state.rawFrameTexture, state.rawFrame);
    updateTexture(&state.processedFrameTexture, state.processedFrame);
    
    // Вычисляем доступное пространство
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float videoWidth = (availableSize.x - 20) / 2.0f;  // Делим на 2 с отступом
    
    // Вычисляем высоту сохраняя aspect ratio
    float aspectRatio = 4.0f / 3.0f;  // По умолчанию 4:3
    if (!state.rawFrame.empty()) {
        aspectRatio = (float)state.rawFrame.cols / (float)state.rawFrame.rows;
    }
    float videoHeight = videoWidth / aspectRatio;
    
    // Входное видео
    ImGui::BeginGroup();
    ImGui::Text("Input Feed");
    if (state.rawFrameTexture != 0 && !state.rawFrame.empty()) {
        ImGui::Image((ImTextureID)(intptr_t)state.rawFrameTexture, ImVec2(videoWidth, videoHeight));
    } else {
        ImGui::Text("No camera feed");
    }
    ImGui::EndGroup();
    
    // Отступ между видео
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    
    // Обработанное видео
    ImGui::BeginGroup();
    ImGui::Text("Processed Output");
    if (state.processedFrameTexture != 0 && !state.processedFrame.empty()) {
        ImGui::Image((ImTextureID)(intptr_t)state.processedFrameTexture, ImVec2(videoWidth, videoHeight));
    } else {
        ImGui::Text("No processed frame");
    }
    ImGui::EndGroup();
    
    ImGui::EndChild();
}
