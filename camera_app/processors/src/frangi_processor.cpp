#include "frangi_processor.h"
#include "mask_filters.h"
#include <iostream>

FrangiProcessor::FrangiProcessor() : glRenderer(nullptr), useGPU(false) {
}

FrangiProcessor::~FrangiProcessor() {
    if (glRenderer) {
        delete glRenderer;
    }
}

bool FrangiProcessor::initialize() {
    // Пытаемся инициализировать GPU рендерер
    if (GLRenderer::isGPUAvailable()) {
        std::cout << "GPU is available, attempting to initialize OpenGL renderer..." << std::endl;
        glRenderer = new GLRenderer();
        if (glRenderer->initialize()) {
            useGPU = true;
            std::cout << "Using GPU (OpenGL) for Frangi processing" << std::endl;
            return true;
        } else {
            std::cout << "Failed to initialize GPU renderer, falling back to CPU" << std::endl;
            delete glRenderer;
            glRenderer = nullptr;
        }
    } else {
        std::cout << "GPU not available (OpenGL 3.3+ required)" << std::endl;
    }
    
    // Fallback на CPU
    useGPU = false;
    std::cout << "Using CPU (OpenCV) for Frangi processing" << std::endl;
    return true;
}

cv::Mat FrangiProcessor::process(const cv::Mat& input, float sigma, float beta, float c,
                                  int displayStage, bool invertEnabled,
                                  bool globalContrastEnabled, float brightness, float contrast,
                                  bool claheEnabled, int claheIterations, float claheTarget,
                                  float segmentationThreshold, float downscaleFactor) {
    // CLAHE всегда применяется на CPU (перед GPU или CPU обработкой)
    cv::Mat preprocessed = input.clone();
    
    if (claheEnabled) {
        MaskFilters filters;
        // Конвертируем в grayscale если нужно
        if (preprocessed.channels() == 3) {
            cv::cvtColor(preprocessed, preprocessed, cv::COLOR_BGR2GRAY);
            preprocessed = filters.applyClahe(preprocessed, claheIterations, claheTarget);
            cv::cvtColor(preprocessed, preprocessed, cv::COLOR_GRAY2BGR);
        } else {
            preprocessed = filters.applyClahe(preprocessed, claheIterations, claheTarget);
        }
    }
    
    if (useGPU && glRenderer) {
        // GPU: Global Contrast через shader, CLAHE уже применен выше
        return glRenderer->processFrame(preprocessed, sigma, beta, c, displayStage, invertEnabled,
                                       globalContrastEnabled, brightness, contrast,
                                       false, 0, 0.0f, segmentationThreshold, downscaleFactor);
    } else {
        // CPU: все через MaskFilters (downscaling не поддерживается на CPU)
        return processCPU(preprocessed, sigma, beta, c, displayStage, invertEnabled,
                         globalContrastEnabled, brightness, contrast,
                         false, 0, 0.0f, segmentationThreshold);  // CLAHE уже применен
    }
}

cv::Mat FrangiProcessor::processCPU(const cv::Mat& input, float sigma, float beta, float c,
                                     int displayStage, bool invertEnabled,
                                     bool globalContrastEnabled, float brightness, float contrast,
                                     bool claheEnabled, int claheIterations, float claheTarget,
                                     float segmentationThreshold) {
    // Препроцессинг на CPU
    cv::Mat preprocessed = input.clone();
    
    if (globalContrastEnabled || claheEnabled) {
        MaskFilters filters;
        
        // Конвертируем в grayscale если нужно
        if (preprocessed.channels() == 3) {
            cv::cvtColor(preprocessed, preprocessed, cv::COLOR_BGR2GRAY);
        }
        
        if (globalContrastEnabled) {
            preprocessed = filters.applyGlobalContrast(preprocessed, brightness, contrast);
        }
        
        if (claheEnabled) {
            preprocessed = filters.applyClahe(preprocessed, claheIterations, claheTarget);
        }
    }
    
    // Конвертируем в grayscale float32
    cv::Mat gray;
    if (preprocessed.channels() == 3) {
        cv::cvtColor(preprocessed, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = preprocessed.clone();
    }
    gray.convertTo(gray, CV_32FC1, 1.0 / 255.0);
    
    // Stage 0: Grayscale
    if (displayStage == 0) {
        cv::Mat result;
        gray.convertTo(result, CV_8U, 255.0);
        return result;
    }
    
    // Stage 1: Invert (optional)
    if (invertEnabled) {
        gray = 1.0f - gray;
    }
    
    if (displayStage == 1) {
        cv::Mat result;
        gray.convertTo(result, CV_8U, 255.0);
        return result;
    }
    
    // Stage 2: Blur
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(0, 0), sigma);
    
    if (displayStage == 2) {
        cv::Mat result;
        blurred.convertTo(result, CV_8U, 255.0);
        return result;
    }
    
    // Stage 3: Gradients (Sobel)
    cv::Mat dx, dy;
    cv::Sobel(blurred, dx, CV_32F, 1, 0, 3);
    cv::Sobel(blurred, dy, CV_32F, 0, 1, 3);
    
    if (displayStage == 3) {
        cv::Mat magnitude;
        cv::magnitude(dx, dy, magnitude);
        magnitude = magnitude * 50.0; // Усиление как в шейдере
        magnitude.convertTo(magnitude, CV_8U, 255.0);
        return magnitude;
    }
    
    // Stages 6-8: Frangi и сегментация
    if (displayStage >= 6) {
        frangi2d_opts_t opts;
        frangi2d_createopts(&opts);
        opts.sigma_start = sigma;
        opts.sigma_end = sigma;
        opts.sigma_step = 1.0;
        opts.Beta = beta;
        opts.C = c;
        opts.BlackWhite = !invertEnabled; // Инвертируем логику
        opts.auto_compute_c = false; // Используем заданный C
        
        cv::Mat vesselness, scale, directions;
        try {
            frangi2d(blurred, vesselness, scale, directions, opts);
        } catch (const std::exception& e) {
            std::cerr << "Frangi filter error: " << e.what() << std::endl;
            return cv::Mat::zeros(input.size(), CV_8U);
        }
        
        // Stage 6: Vesselness (raw probabilities with enhancement for visualization)
        if (displayStage == 6) {
            cv::Mat result;
            vesselness = vesselness * 100.0;
            vesselness = vesselness.mul(vesselness);
            vesselness.convertTo(result, CV_8U, 255.0);
            return result;
        }
        
        // Stage 7: Segmentation (binary mask)
        cv::Mat segmented;
        cv::threshold(vesselness, segmented, segmentationThreshold, 1.0, cv::THRESH_BINARY);
        
        if (displayStage == 7) {
            cv::Mat result;
            segmented.convertTo(result, CV_8U, 255.0);
            return result;
        }
        
        // Stage 8: Overlay (binary mask on original)
        if (displayStage == 8) {
            cv::Mat result;
            
            // Конвертируем оригинал в float RGB
            cv::Mat original;
            if (input.channels() == 3) {
                input.convertTo(original, CV_32F, 1.0 / 255.0);
            } else {
                cv::Mat temp;
                cv::cvtColor(input, temp, cv::COLOR_GRAY2BGR);
                temp.convertTo(original, CV_32F, 1.0 / 255.0);
            }
            
            // Добавляем маску (белые вены на оригинале)
            cv::Mat mask3ch;
            cv::merge(std::vector<cv::Mat>(3, segmented), mask3ch);
            original += mask3ch;
            
            original.convertTo(result, CV_8U, 255.0);
            return result;
        }
    }
    
    // Stages 4-5: вычисляем промежуточные результаты
    cv::Mat Dxx, Dxy, Dyy;
    frangi2d_hessian(blurred, Dxx, Dxy, Dyy, sigma);
    Dxx = Dxx * sigma * sigma;
    Dxy = Dxy * sigma * sigma;
    Dyy = Dyy * sigma * sigma;
    
    // Stage 4: Hessian
    if (displayStage == 4) {
        cv::Mat result;
        cv::Mat absHessian;
        cv::absdiff(Dxx, cv::Scalar(0), absHessian);
        absHessian = absHessian * 10.0; // Усиление
        absHessian.convertTo(result, CV_8U, 255.0);
        return result;
    }
    
    // Stage 5: Eigenvalues
    cv::Mat lambda1, lambda2, Ix, Iy;
    frangi2_eig2image(Dxx, Dxy, Dyy, lambda1, lambda2, Ix, Iy);
    
    cv::Mat result;
    cv::Mat absEigen;
    cv::absdiff(lambda1, cv::Scalar(0), absEigen);
    absEigen = absEigen * 10.0; // Усиление
    absEigen.convertTo(result, CV_8U, 255.0);
    
    return result;
}

