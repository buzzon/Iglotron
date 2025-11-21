#ifndef FRANGI_PROCESSOR_H
#define FRANGI_PROCESSOR_H

#include <opencv2/opencv.hpp>
#include "gl_renderer.h"
#include "frangi.h"

class FrangiProcessor {
public:
    FrangiProcessor();
    ~FrangiProcessor();
    
    // Инициализация (пытается использовать GPU, fallback на CPU)
    bool initialize();
    
    // Обработка кадра (автоматически выбирает GPU или CPU)
    cv::Mat process(const cv::Mat& input, float sigma, float beta, float c,
                    int displayStage, bool invertEnabled,
                    bool globalContrastEnabled, float brightness, float contrast,
                    bool claheEnabled, int claheIterations, float claheTarget,
                    float segmentationThreshold, float downscaleFactor = 1.0f);
    
    // Проверка используется ли GPU
    bool isUsingGPU() const { return useGPU; }
    
    // Получить текущий метод обработки
    const char* getMethodName() const { return useGPU ? "GPU (OpenGL)" : "CPU (OpenCV)"; }
    
    // Получить размеры downscaled изображения (только для GPU)
    int getDownscaledWidth() const { return (useGPU && glRenderer) ? glRenderer->getDownscaledWidth() : 0; }
    int getDownscaledHeight() const { return (useGPU && glRenderer) ? glRenderer->getDownscaledHeight() : 0; }

private:
    GLRenderer* glRenderer;
    bool useGPU;
    
    // CPU обработка через готовый frangi.cpp
    cv::Mat processCPU(const cv::Mat& input, float sigma, float beta, float c,
                       int displayStage, bool invertEnabled,
                       bool globalContrastEnabled, float brightness, float contrast,
                       bool claheEnabled, int claheIterations, float claheTarget,
                       float segmentationThreshold);
};

#endif // FRANGI_PROCESSOR_H

