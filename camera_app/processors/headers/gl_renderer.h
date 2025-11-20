#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <glad/glad.h>
#include <opencv2/opencv.hpp>
#include <string>

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();
    
    // Инициализация OpenGL ресурсов
    bool initialize();
    
    // Обработка кадра на GPU
    cv::Mat processFrame(const cv::Mat& input, float sigma, float beta, float c, 
                         int displayStage, bool invertEnabled,
                         bool globalContrastEnabled, float brightness, float contrast,
                         bool claheEnabled, int claheIterations, float claheTarget,
                         float segmentationThreshold);
    
    // Проверка доступности OpenGL
    static bool isGPUAvailable();
    
    // Освобождение ресурсов
    void cleanup();

private:
    // Шейдерные программы
    GLuint globalContrastShader;
    GLuint grayscaleShader;
    GLuint invertShader;
    GLuint blurXShader;
    GLuint blurYShader;
    GLuint gradientsShader;
    GLuint hessianShader;
    GLuint eigenvaluesShader;
    GLuint vesselnessShader;
    GLuint segmentationShader;
    GLuint overlayShader;
    GLuint visualizeShader;
    
    // Framebuffers
    GLuint fboPreprocessed;
    GLuint fboGray;
    GLuint fboInvert;
    GLuint fboBlurX;
    GLuint fboBlurY;
    GLuint fboGradients;
    GLuint fboHessian;
    GLuint fboEigenvalues;
    GLuint fboVesselness;
    GLuint fboSegmentation;
    GLuint fboOverlay;
    
    // Текстуры для framebuffers
    GLuint texPreprocessed;
    GLuint texGray;
    GLuint texInvert;
    GLuint texBlurX;
    GLuint texBlurY;
    GLuint texGradients;
    GLuint texHessian;
    GLuint texEigenvalues;
    GLuint texVesselness;
    GLuint texSegmentation;
    GLuint texOverlay;
    
    // Входная текстура
    GLuint inputTexture;
    
    // VAO и VBO для quad
    GLuint vao;
    GLuint vbo;
    
    // Размеры текущего кадра
    int currentWidth;
    int currentHeight;
    
    // Вспомогательные методы
    GLuint compileShader(const char* vertexSrc, const char* fragmentSrc);
    void createFramebuffer(GLuint* fbo, GLuint* texture, int width, int height);
    void renderPass(GLuint program, GLuint targetFBO, GLuint inputTex);
    void recreateFramebuffers(int width, int height);
    void uploadTexture(const cv::Mat& image);
    cv::Mat downloadTexture(GLuint texture, int width, int height);
    
    bool initialized;
};

#endif // GL_RENDERER_H

