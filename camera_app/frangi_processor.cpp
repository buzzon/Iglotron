#include "frangi_processor.h"
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
                                  int displayStage, bool invertEnabled) {
    if (useGPU && glRenderer) {
        return glRenderer->processFrame(input, sigma, beta, c, displayStage, invertEnabled);
    } else {
        return processCPU(input, sigma, beta, c, displayStage, invertEnabled);
    }
}

cv::Mat FrangiProcessor::processCPU(const cv::Mat& input, float sigma, float beta, float c,
                                     int displayStage, bool invertEnabled) {
    // Конвертируем в grayscale float32
    cv::Mat gray;
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input.clone();
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
    
    // Stage 6: Vesselness - используем вашу готовую реализацию frangi2d()
    if (displayStage == 6) {
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
        
        // Усиление и контраст (как в GPU версии)
        cv::Mat result;
        vesselness = vesselness * 100.0;
        vesselness = vesselness.mul(vesselness);
        vesselness.convertTo(result, CV_8U, 255.0);
        
        return result;
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

