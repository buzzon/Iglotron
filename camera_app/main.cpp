#include <opencv2/opencv.hpp>
#include <cmath>

#include "app_state.h"
#include "processors/headers/frangi_processor.h"
#include "processors/headers/mask_filters.h"
#include "settings/headers/settings.h"
#include "managers/headers/window_manager.h"
#include "managers/headers/camera_manager.h"
#include "managers/headers/gui_manager.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

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
    
    state.downscaleDivisor = state.settings.downscaleDivisor;
    
    state.cameraManager.setSelectedCameraIndex(state.settings.selectedCameraIndex);
    
    state.approvalEnabled = state.settings.approvalEnabled;
    state.approvalMaskHeight = state.settings.approvalMaskHeight;
    state.approvalMaskWidth = state.settings.approvalMaskWidth;
    state.approvalThreshold = state.settings.approvalThreshold;
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
    
    state.settings.downscaleDivisor = state.downscaleDivisor;
    
    state.settings.selectedCameraIndex = state.cameraManager.getSelectedCameraIndex();
    
    state.settings.approvalEnabled = state.approvalEnabled;
    state.settings.approvalMaskHeight = state.approvalMaskHeight;
    state.settings.approvalMaskWidth = state.approvalMaskWidth;
    state.settings.approvalThreshold = state.approvalThreshold;
};

// Вычислить координаты маски аппрувинга (центр-снизу)
void computeApprovalMaskCoords(int imageHeight, int imageWidth, 
                               int maskHeight, int maskWidth,
                               int& y, int& x) {
    y = imageHeight - maskHeight;
    x = (imageWidth - maskWidth) / 2;
}

// Вычислить соотношение сосудов в маске аппрувинга
float computeApprovalRatio(const cv::Mat& segmentedFrame, 
                           int maskY, int maskX, 
                           int maskHeight, int maskWidth) {
    if (segmentedFrame.empty() || 
        maskY < 0 || maskX < 0 ||
        maskY + maskHeight > segmentedFrame.rows ||
        maskX + maskWidth > segmentedFrame.cols) {
        return 0.0f;
    }
    
    // Берем ROI из сегментированного кадра (бинарная маска)
    cv::Rect roi(maskX, maskY, maskWidth, maskHeight);
    cv::Mat maskRegion = segmentedFrame(roi);
    
    // Считаем белые пиксели (вены)
    int vesselsPixels = cv::countNonZero(maskRegion);
    int totalPixels = maskRegion.rows * maskRegion.cols;
    
    return totalPixels > 0 ? static_cast<float>(vesselsPixels) / totalPixels : 0.0f;
}

// Нарисовать маску аппрувинга на кадре
void drawApprovalMask(cv::Mat& frame, int maskY, int maskX, 
                      int maskHeight, int maskWidth, 
                      float ratio, float threshold) {
    if (frame.empty() || 
        maskY < 0 || maskX < 0 ||
        maskY + maskHeight > frame.rows ||
        maskX + maskWidth > frame.cols) {
        return;
    }
    
    cv::Rect rect(maskX, maskY, maskWidth, maskHeight);
    cv::Mat roi = frame(rect);
    
    // Определяем цвет: зеленый если approved, серый если нет
    cv::Scalar tintColor;
    if (ratio >= threshold) {
        tintColor = cv::Scalar(0, 255, 0);  // Зеленый (BGR) - approved
    } else {
        tintColor = cv::Scalar(128, 128, 128);  // Серый (BGR) - не approved
    }
    
    // Применяем цветовой оттенок к области
    cv::Mat coloredRoi = roi.clone();
    cv::Mat tintMask(roi.size(), roi.type(), tintColor);
    
    // Смешивание: 70% оригинал + 30% цветовой оттенок
    double alpha = 0.3;
    cv::addWeighted(coloredRoi, 1.0 - alpha, tintMask, alpha, 0, roi);
    
    // Рисуем границу маски
    cv::Scalar borderColor = (ratio >= threshold) ? 
        cv::Scalar(0, 255, 0) :    // Зеленая граница
        cv::Scalar(128, 128, 128); // Серая граница
    cv::rectangle(frame, rect, borderColor, 3);
    
    // Добавляем текст с процентом и статусом
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << (ratio * 100.0f) << "%";
    std::string text = ss.str();
    
    // Статус текст
    std::string statusText = (ratio >= threshold) ? "APPROVED" : "NOT SAFE";
    
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
    cv::Size statusSize = cv::getTextSize(statusText, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
    
    // Позиция для процентов (центр маски)
    cv::Point textPos(maskX + (maskWidth - textSize.width) / 2, 
                     maskY + (maskHeight - 10) / 2);
    
    // Позиция для статуса (под процентами)
    cv::Point statusPos(maskX + (maskWidth - statusSize.width) / 2,
                       maskY + (maskHeight + statusSize.height) / 2 + 15);
    
    // Текст с тенью для читаемости - проценты
    cv::putText(frame, text, textPos + cv::Point(2, 2), 
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 4);
    cv::putText(frame, text, textPos, 
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
    
    // Текст статуса
    cv::putText(frame, statusText, statusPos + cv::Point(2, 2), 
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 3);
    cv::putText(frame, statusText, statusPos, 
                cv::FONT_HERSHEY_SIMPLEX, 0.6, borderColor, 2);
};


void updateFPS(AppState& state, WindowManager& windowManager) {
    double currentTime = windowManager.getTime();
    state.frameCount++;
    
    if (currentTime - state.lastTime >= 1.0) {
        state.fps = state.frameCount / (currentTime - state.lastTime);
        state.frameCount = 0;
        state.lastTime = currentTime;
    }
}

void processFrame(AppState& state, MaskFilters& filters) {
    if (!state.cameraManager.isOpen()) {
        return;
    }
    
    // Захват кадра
    if (!state.cameraManager.grabFrame(state.rawFrame)) {
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
            state.segmentationThreshold,
            1.0f / static_cast<float>(state.downscaleDivisor)  // Вычисляем factor из divisor
        );
    }
    
    // Аппрувинг (если включен)
    if (state.approvalEnabled && !state.processedFrame.empty()) {
        // Получаем сегментированную маску (stage 7)
        cv::Mat segmentedMask;
        if (state.processor) {
            segmentedMask = state.processor->process(
                state.rawFrame, 
                state.sigma, 
                state.beta, 
                state.c,
                6,  // Stage 6 = Segmentation (исправлено: было 7, должно быть 6)
                state.invertEnabled,
                state.globalContrastEnabled,
                state.globalBrightness,
                state.globalContrast,
                state.claheEnabled,
                state.claheMaxIterations,
                state.claheTargetContrast,
                state.segmentationThreshold,
                1.0f / static_cast<float>(state.downscaleDivisor)  // Вычисляем factor из divisor
            );
        }
        
        if (!segmentedMask.empty()) {
            // Масштабируем координаты и размеры маски для downscaled разрешения
            float downscaleFactor = 1.0f / static_cast<float>(state.downscaleDivisor);
            int downscaledMaskHeight = static_cast<int>(std::round(state.approvalMaskHeight * downscaleFactor));
            int downscaledMaskWidth = static_cast<int>(std::round(state.approvalMaskWidth * downscaleFactor));
            
            // Вычисляем координаты маски для downscaled разрешения
            int maskY, maskX;
            computeApprovalMaskCoords(segmentedMask.rows, segmentedMask.cols,
                                     downscaledMaskHeight, downscaledMaskWidth,
                                     maskY, maskX);
            
            // Вычисляем соотношение сосудов в downscaled маске
            state.approvalRatio = computeApprovalRatio(segmentedMask, maskY, maskX,
                                                       downscaledMaskHeight, 
                                                       downscaledMaskWidth);
        } else {
            state.approvalRatio = 0.0f;
        }
        
        // Рисуем маску на обработанном кадре (используем оригинальные размеры)
        int maskY, maskX;
        computeApprovalMaskCoords(state.processedFrame.rows, state.processedFrame.cols,
                                 state.approvalMaskHeight, state.approvalMaskWidth,
                                 maskY, maskX);
        drawApprovalMask(state.processedFrame, maskY, maskX,
                        state.approvalMaskHeight, state.approvalMaskWidth,
                        state.approvalRatio, state.approvalThreshold);
    }
}


void cleanup(AppState& state) {
    // Освобождаем ресурсы
    if (state.processor) {
        delete state.processor;
    }
    
    // Камера закрывается автоматически в деструкторе CameraManager
    
    // Удаляем ImGui текстуры
    if (state.rawFrameTexture) {
        glDeleteTextures(1, &state.rawFrameTexture);
    }
    if (state.processedFrameTexture) {
        glDeleteTextures(1, &state.processedFrameTexture);
    }
}

int main() {
    std::cout << "=== Frangi Filter Camera Application ===" << std::endl;
    std::cout << "Initializing..." << std::endl;
    
    AppState state;
    WindowManager windowManager;
    GUIManager guiManager;
    MaskFilters maskFilters;
    
    // Загрузка настроек
    std::cout << "Loading settings..." << std::endl;
    SettingsManager::loadSettings("settings/configs/settings.json", state.settings);
    loadSettingsToState(state);
    
    // Инициализация окна (GLFW + GLAD)
    if (!windowManager.initialize(1600, 900, "Frangi Filter - Camera App")) {
        return -1;
    }
    
    // Инициализация GUI Manager
    guiManager.initialize(windowManager.getWindow());
    
    // Инициализация Frangi процессора
    state.processor = new FrangiProcessor();
    if (!state.processor->initialize()) {
        std::cerr << "Failed to initialize Frangi processor" << std::endl;
        guiManager.shutdown();
        windowManager.shutdown();
        cleanup(state);
        return -1;
    }
    
    // Инициализация камеры
    state.cameraManager.scanAvailableCameras();
    if (!state.cameraManager.openCamera(state.cameraManager.getSelectedCameraIndex())) {
        std::cerr << "Failed to initialize camera" << std::endl;
        guiManager.shutdown();
        windowManager.shutdown();
        cleanup(state);
        return -1;
    }
    
    std::cout << "Initialization complete!" << std::endl;
    std::cout << "Using " << state.processor->getMethodName() << " for processing" << std::endl;
    
    state.lastTime = windowManager.getTime();
    
    // Главный цикл
    while (!windowManager.shouldClose()) {
        windowManager.pollEvents();
        
        // Обработка кадра с камеры
        processFrame(state, maskFilters);
        
        // Обновление FPS
        updateFPS(state, windowManager);
        
        // Рендеринг GUI через ImGui
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        guiManager.render(state);
        windowManager.swapBuffers();
    }
    
    std::cout << "Shutting down..." << std::endl;
    guiManager.shutdown();
    windowManager.shutdown();
    cleanup(state);
    
    std::cout << "Application terminated successfully" << std::endl;
    return 0;
}
