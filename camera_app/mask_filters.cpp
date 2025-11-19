#include "mask_filters.h"
#include <iostream>

MaskFilters::MaskFilters() {
    // Создаем CLAHE объект с начальными параметрами
    // clipLimit = 2.0, tileGridSize = (6, 6) - как в Python коде
    m_clahe = cv::createCLAHE(2.0, cv::Size(6, 6));
    std::cout << "MaskFilters initialized" << std::endl;
}

MaskFilters::~MaskFilters() {
}

cv::Mat MaskFilters::applyGlobalContrast(const cv::Mat& image, float brightness, float contrast) {
    // Проверка входного изображения
    if (image.empty()) {
        std::cerr << "applyGlobalContrast: empty input image" << std::endl;
        return cv::Mat();
    }
    
    // Конвертация в float для точных вычислений
    cv::Mat image_float;
    image.convertTo(image_float, CV_32F);
    
    // Центрирование относительно 128
    cv::Mat centered = image_float - 128.0f;
    
    // Масштабирование контраста
    cv::Mat scaled = centered * contrast;
    
    // Добавление яркости и возврат к 128
    cv::Mat result = scaled + 128.0f + brightness;
    
    // Клиппинг в диапазон [0, 255]
    cv::Mat clipped;
    cv::threshold(result, clipped, 255.0, 255.0, cv::THRESH_TRUNC);
    cv::threshold(clipped, clipped, 0.0, 0.0, cv::THRESH_TOZERO);
    
    // Конвертация обратно в uint8
    cv::Mat output;
    clipped.convertTo(output, CV_8U);
    
    return output;
}

float MaskFilters::calculateContrast(const cv::Mat& image) {
    // Вычисление среднего и стандартного отклонения
    cv::Scalar mean, stddev;
    cv::meanStdDev(image, mean, stddev);
    
    // Контраст = std / mean (с защитой от деления на 0)
    float contrast = stddev[0] / (mean[0] + 1e-6f);
    return contrast;
}

cv::Mat MaskFilters::applyClahe(const cv::Mat& image, int max_iterations, float target_contrast) {
    // Проверка входного изображения
    if (image.empty()) {
        std::cerr << "applyClahe: empty input image" << std::endl;
        return cv::Mat();
    }
    
    // Копируем изображение для обработки
    cv::Mat enhanced = image.clone();
    
    // Вычисляем начальный контраст
    float contrast = calculateContrast(enhanced);
    
    // Итеративно применяем CLAHE
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        // Применяем CLAHE
        m_clahe->apply(enhanced, enhanced);
        
        // Вычисляем новый контраст
        contrast = calculateContrast(enhanced);
        
        // Проверяем достижение цели
        if (contrast >= target_contrast) {
            std::cout << "CLAHE: target contrast reached after " << (iteration + 1) 
                      << " iterations (contrast = " << contrast << ")" << std::endl;
            break;
        }
        
        // Если контраст слишком низкий, увеличиваем clipLimit
        if (contrast < target_contrast * 0.5f) {
            double current_clip = m_clahe->getClipLimit();
            double new_clip = std::min(4.0, current_clip * 1.2);
            m_clahe->setClipLimit(new_clip);
            std::cout << "CLAHE: adjusting clipLimit to " << new_clip << std::endl;
        }
    }
    
    return enhanced;
}

cv::Mat MaskFilters::applySegmentation(const cv::Mat& image, float threshold) {
    // Проверка входного изображения
    if (image.empty()) {
        std::cerr << "applySegmentation: empty input image" << std::endl;
        return cv::Mat();
    }
    
    int rows = image.rows;
    int cols = image.cols;
    
    // Создаем бинарную маску
    cv::Mat masked_vessels = cv::Mat::zeros(rows, cols, CV_8U);
    
    // Пороговая сегментация: >= threshold -> 255, < threshold -> 0
    for (int i = 0; i < rows; ++i) {
        const uchar* src_ptr = image.ptr<uchar>(i);
        uchar* dst_ptr = masked_vessels.ptr<uchar>(i);
        
        for (int j = 0; j < cols; ++j) {
            if (src_ptr[j] >= threshold) {
                dst_ptr[j] = 255;
            } else {
                dst_ptr[j] = 0;
            }
        }
    }
    
    return masked_vessels;
}

