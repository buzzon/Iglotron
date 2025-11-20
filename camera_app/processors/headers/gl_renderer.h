#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <glad/glad.h>
#include <opencv2/opencv.hpp>
#include <vector>

/**
 * @brief GLRenderer - GPU-ускоренная реализация фильтра Frangi с правильным вычислением Гессиана
 * 
 * Основные улучшения по сравнению с предыдущей версией:
 * 1. Использует производные Гауссиана (аналитические ядра) вместо конечных разностей
 * 2. Применяет scale normalization (умножение на σ²)
 * 3. Сепарабельные свертки для эффективности
 * 4. Поддержка как темных, так и светлых вен
 */
class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();
    
    /**
     * @brief Проверяет доступность GPU и поддержку OpenGL 3.3+
     */
    static bool isGPUAvailable();
    
    /**
     * @brief Инициализирует OpenGL контекст, компилирует шейдеры
     * @return true если инициализация успешна
     */
    bool initialize();
    
    /**
     * @brief Обрабатывает один кадр через весь пайплайн Frangi
     * 
     * @param input Входное изображение (цветное или grayscale)
     * @param sigma Масштаб для производных Гауссиана (оптимально: диаметр_вены / 2.83)
     * @param beta Параметр фильтра Frangi для подавления blob-структур (типично 0.5)
     * @param c Параметр фильтра Frangi для подавления шума (типично 15-50, зависит от контраста)
     * @param displayStage Какой этап пайплайна показать:
     *        0 = Grayscale
     *        1 = Invert
     *        2 = Dxx (отладка)
     *        3 = Hessian (после scale normalization)
     *        4 = Eigenvalues
     *        5 = Vesselness
     *        6 = Segmentation (бинарная маска)
     *        7/8 = Overlay (маска на оригинале)
     * @param invertEnabled Инвертировать изображение (темные вены vs светлые вены)
     * @param globalContrastEnabled Применить глобальное изменение яркости/контраста
     * @param brightness Яркость (-255 до 255)
     * @param contrast Контраст (0.5 до 3.0)
     * @param claheEnabled (не используется в GPU, применяется на CPU)
     * @param claheIterations (не используется)
     * @param claheTarget (не используется)
     * @param segmentationThreshold Порог для бинаризации vesselness (0.0 до 1.0)
     * 
     * @return Обработанное изображение в формате CV_8U
     */
    cv::Mat processFrame(const cv::Mat& input, 
                         float sigma, 
                         float beta, 
                         float c,
                         int displayStage, 
                         bool invertEnabled,
                         bool globalContrastEnabled, 
                         float brightness, 
                         float contrast,
                         bool claheEnabled, 
                         int claheIterations, 
                         float claheTarget,
                         float segmentationThreshold);
    
    /**
     * @brief Освобождает все OpenGL ресурсы
     */
    void cleanup();

private:
    // Компиляция шейдеров
    GLuint compileShader(const char* vertexSrc, const char* fragmentSrc);
    
    // Создание framebuffer + текстуры
    void createFramebuffer(GLuint* fbo, GLuint* texture, int width, int height);
    
    // Пересоздание всех framebuffers при изменении размера
    void recreateFramebuffers(int width, int height);
    
    // Загрузка OpenCV Mat в OpenGL текстуру
    void uploadTexture(const cv::Mat& image);
    
    // Скачивание OpenGL текстуры в OpenCV Mat
    cv::Mat downloadTexture(GLuint texture, int width, int height);
    
    // Рендер одного прохода (простой случай: один вход, один выход)
    void renderPass(GLuint program, GLuint targetFBO, GLuint inputTex);
    
    // Установка параметров для шейдера свертки
    void setConvolveKernel(const std::vector<float>& kernel, int direction);
    
    // Флаги состояния
    bool initialized;
    int currentWidth;
    int currentHeight;
    
    // Shader programs
    GLuint globalContrastShader;
    GLuint grayscaleShader;
    GLuint invertShader;
    GLuint convolve1DShader;      // Универсальный шейдер 1D свертки
    GLuint scaleNormShader;       // Scale normalization (σ²)
    GLuint eigenvaluesShader;
    GLuint vesselnessShader;
    GLuint segmentationShader;
    GLuint overlayShader;
    
    // Framebuffers
    GLuint fboPreprocessed;
    GLuint fboGray;
    GLuint fboInvert;
    
    // Временные буферы для вычисления производных
    GLuint fboDxxTemp;  // После первого прохода d²G/dx²
    GLuint fboDxx;      // Финальный Dxx после второго прохода G
    GLuint fboDyyTemp;  // После первого прохода G
    GLuint fboDyy;      // Финальный Dyy после второго прохода d²G/dy²
    GLuint fboDxyTemp;  // После первого прохода dG/dx
    GLuint fboDxy;      // Финальный Dxy после второго прохода dG/dy
    
    GLuint fboHessian;       // После scale normalization
    GLuint fboEigenvalues;
    GLuint fboVesselness;
    GLuint fboSegmentation;
    GLuint fboOverlay;
    
    // Textures (соответствуют framebuffers)
    GLuint texPreprocessed;
    GLuint texGray;
    GLuint texInvert;
    
    GLuint texDxxTemp;
    GLuint texDxx;
    GLuint texDyyTemp;
    GLuint texDyy;
    GLuint texDxyTemp;
    GLuint texDxy;
    
    GLuint texHessian;
    GLuint texEigenvalues;
    GLuint texVesselness;
    GLuint texSegmentation;
    GLuint texOverlay;
    
    // Входная текстура (для оригинального изображения)
    GLuint inputTexture;
    
    // VAO и VBO для полноэкранного quad
    GLuint vao;
    GLuint vbo;
};

#endif // GL_RENDERER_H
