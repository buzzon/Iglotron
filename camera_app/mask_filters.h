#ifndef MASK_FILTERS_H
#define MASK_FILTERS_H

#include <opencv2/opencv.hpp>

/**
 * @brief Класс для применения различных фильтров маски и обработки изображений
 * 
 * Содержит методы для контрастирования, адаптивной гистограммной эквализации
 * и сегментации сосудов на NIR изображениях
 */
class MaskFilters {
public:
    MaskFilters();
    ~MaskFilters();
    
    /**
     * @brief Применяет глобальное изменение контраста и яркости
     * 
     * Формула: result = (image - 128) * contrast + 128 + brightness
     * 
     * @param image Входное изображение (grayscale)
     * @param brightness Яркость (обычно от -100 до +100)
     * @param contrast Контраст (обычно от 0.5 до 2.0, 1.0 = без изменений)
     * @return Обработанное изображение
     */
    cv::Mat applyGlobalContrast(const cv::Mat& image, float brightness = 0.0f, float contrast = 1.0f);
    
    /**
     * @brief Применяет CLAHE (Contrast Limited Adaptive Histogram Equalization)
     * 
     * Адаптивно усиливает контраст до достижения целевого значения.
     * Итеративно применяет CLAHE, корректируя параметры на лету.
     * 
     * @param image Входное изображение (grayscale)
     * @param max_iterations Максимальное количество итераций (по умолчанию 5)
     * @param target_contrast Целевой контраст (std/mean), по умолчанию 0.3
     * @return Контрастированное изображение
     */
    cv::Mat applyClahe(const cv::Mat& image, int max_iterations = 5, float target_contrast = 0.3f);
    
    /**
     * @brief Применяет пороговую сегментацию
     * 
     * Бинаризация: пиксели >= порога = 255 (белый), < порога = 0 (черный)
     * 
     * @param image Входное изображение (обычно vesselness или filtered image)
     * @param threshold Порог сегментации (0-255)
     * @return Бинарная маска
     */
    cv::Mat applySegmentation(const cv::Mat& image, float threshold);
    
private:
    cv::Ptr<cv::CLAHE> m_clahe;  ///< CLAHE объект OpenCV
    
    /**
     * @brief Вычисляет контраст изображения как std/mean
     * @param image Входное изображение
     * @return Значение контраста
     */
    float calculateContrast(const cv::Mat& image);
};

#endif // MASK_FILTERS_H

