#ifndef APP_STATE_H
#define APP_STATE_H

#include <opencv2/opencv.hpp>
#include "managers/headers/camera_manager.h"
#include "settings/headers/settings.h"

// Forward declarations
class FrangiProcessor;
typedef unsigned int GLuint;

/**
 * @brief Глобальное состояние приложения
 * 
 * Содержит все параметры, данные и состояние приложения
 */
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
    CameraManager cameraManager;
    cv::Mat rawFrame;
    cv::Mat preprocessedFrame;  // После препроцессинга
    cv::Mat processedFrame;      // После Frangi
    
    // Frangi процессор
    FrangiProcessor* processor = nullptr;
    
    // FPS
    double lastTime = 0.0;
    int frameCount = 0;
    float fps = 0.0f;
    
    // ImGui текстуры для отображения видео
    GLuint rawFrameTexture = 0;
    GLuint processedFrameTexture = 0;
    
    // Аппрувинг (injection window)
    bool approvalEnabled;
    int approvalMaskHeight;
    int approvalMaskWidth;
    float approvalThreshold;
    float approvalRatio = 0.0f;  // Текущее соотношение сосудов
    
    // Настройки
    AppSettings settings;
};

#endif // APP_STATE_H

