#include "gl_renderer.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>

// Windows (MSVC)
#define _USE_MATH_DEFINES
#include <math.h>

// Fallback
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// ============================================================================
// SHADER SOURCES
// ============================================================================

// Вершинный шейдер (общий для всех проходов)
const char* vertexShaderSrc = R"(
#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

out vec2 vUv;

void main() {
    vUv = texCoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

// Global Contrast shader (препроцессинг)
const char* globalContrastFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uBrightness;
uniform float uContrast;

void main() {
    vec4 color = texture(uTexture, vUv);
    
    // Центрируем относительно 0.5 (в нормализованных координатах)
    vec3 centered = color.rgb - 0.5;
    
    // Масштабируем контраст
    vec3 scaled = centered * uContrast;
    
    // Добавляем яркость и возвращаем к 0.5
    vec3 result = scaled + 0.5 + (uBrightness / 255.0);
    
    // Клиппинг в диапазон [0, 1]
    result = clamp(result, 0.0, 1.0);
    
    FragColor = vec4(result, 1.0);
}
)";

// Grayscale shader
const char* grayscaleFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 color = texture(uTexture, vUv);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(gray, gray, gray, 1.0);
}
)";

// Invert shader
const char* invertFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 color = texture(uTexture, vUv);
    float inverted = 1.0 - color.x;
    FragColor = vec4(inverted, inverted, inverted, 1.0);
}
)";

// Downscale shader - уменьшает разрешение с билинейной интерполяцией
const char* downscaleFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    // Просто берем текстуру с билинейной интерполяцией
    // OpenGL автоматически интерполирует при рендеринге в меньший framebuffer
    FragColor = texture(uTexture, vUv);
}
)";

// ============================================================================
// НОВЫЙ УНИВЕРСАЛЬНЫЙ ШЕЙДЕР 1D СВЕРТКИ
// ============================================================================

const char* convolve1DFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uKernel[64];  // Максимум 64 коэффициента (до sigma=10)
uniform int uKernelSize;
uniform int uDirection;  // 0 = horizontal, 1 = vertical

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    vec2 onePixel = 1.0 / texSize;
    
    int halfSize = uKernelSize / 2;
    float result = 0.0;
    
    for (int i = 0; i < uKernelSize; i++) {
        int offset = i - halfSize;
        vec2 samplePos;
        
        if (uDirection == 0) {
            // Horizontal
            samplePos = vUv + vec2(float(offset) * onePixel.x, 0.0);
        } else {
            // Vertical
            samplePos = vUv + vec2(0.0, float(offset) * onePixel.y);
        }
        
        float sample = texture(uTexture, samplePos).x;
        result += sample * uKernel[i];
    }
    
    FragColor = vec4(result, 0.0, 0.0, 1.0);
}
)";

// ============================================================================
// SCALE NORMALIZATION SHADER
// ============================================================================

const char* scaleNormalizationFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uDxx;
uniform sampler2D uDyy;
uniform sampler2D uDxy;
uniform float uSigma;

void main() {
    float dxx = texture(uDxx, vUv).x;
    float dyy = texture(uDyy, vUv).x;
    float dxy = texture(uDxy, vUv).x;
    
    // Scale normalization (gamma = 2)
    float sigma2 = uSigma * uSigma;
    
    dxx *= sigma2;
    dyy *= sigma2;
    dxy *= sigma2;
    
    // Выходной формат: RGB = (Dxx, Dxy, Dyy)
    FragColor = vec4(dxx, dxy, dyy, 1.0);
}
)";

// ============================================================================
// EIGENVALUES SHADER (исправленный)
// ============================================================================

const char* eigenvaluesFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;  // (Dxx, Dxy, Dyy)

void main() {
    vec3 hessian = texture(uTexture, vUv).xyz;
    float dxx = hessian.x;
    float dxy = hessian.y;
    float dyy = hessian.z;
    
    // Аналитические собственные значения 2x2 симметричной матрицы
    // lambda = 0.5 * (Dxx + Dyy ± sqrt((Dxx - Dyy)² + 4*Dxy²))
    
    float trace = dxx + dyy;
    float det = dxx * dyy - dxy * dxy;
    float discriminant = trace * trace - 4.0 * det;
    
    // Защита от отрицательного дискриминанта (численная ошибка)
    discriminant = max(discriminant, 0.0);
    float sqrtDisc = sqrt(discriminant);
    
    float lambda1 = 0.5 * (trace - sqrtDisc);  // Меньшее по модулю
    float lambda2 = 0.5 * (trace + sqrtDisc);  // Большее по модулю
    
    // Сортировка по абсолютному значению: |lambda1| <= |lambda2|
    if (abs(lambda1) > abs(lambda2)) {
        float temp = lambda1;
        lambda1 = lambda2;
        lambda2 = temp;
    }
    
    FragColor = vec4(lambda1, lambda2, 0.0, 1.0);
}
)";

// ============================================================================
// VESSELNESS SHADER (исправленный с поддержкой dark/bright vessels)
// ============================================================================

const char* vesselnessFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;  // (lambda1, lambda2)
uniform float uBeta;
uniform float uC;
uniform int uBlackWhite;  // 1 = темные вены (черные на белом), 0 = светлые вены (белые на черном)

void main() {
    vec4 eigs = texture(uTexture, vUv);
    float lambda1 = eigs.x;
    float lambda2 = eigs.y;
    
    float vesselness = 0.0;
    
    // Проверка знака lambda2
    // Для темных вен: lambda2 > 0 (выпуклая кривизна)
    // Для светлых вен: lambda2 < 0 (вогнутая кривизна)
    bool is_vessel = (uBlackWhite == 1) ? (lambda2 > 0.0) : (lambda2 < 0.0);
    
    if (is_vessel) {
        float beta_sq = uBeta * uBeta;
        float c_sq = uC * uC;
        
        // Защита от деления на ноль
        float lambda2_safe = (abs(lambda2) < 1e-10) ? 
            ((uBlackWhite == 1) ? 1e-10 : -1e-10) : lambda2;
        
        // Rb: отношение собственных значений в квадрате
        float rb = lambda1 / lambda2_safe;
        rb = rb * rb;
        
        // S: норма Фробениуса гессиана
        float s2 = lambda1 * lambda1 + lambda2_safe * lambda2_safe;
        
        // Frangi formula (из оригинальной статьи)
        // term1: подавляет blob-like структуры (близко к 1 для линий)
        // term2: подавляет фоновый шум (близко к 1 для структур)
        float term1 = exp(-rb / (2.0 * beta_sq));
        float term2 = 1.0 - exp(-s2 / (2.0 * c_sq));
        
        vesselness = term1 * term2;
    }
    
    FragColor = vec4(vec3(vesselness), 1.0);
}
)";

// Segmentation shader - бинаризация по порогу
const char* segmentationFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uThreshold;

void main() {
    float vesselness = texture(uTexture, vUv).x;
    
    // Простая бинаризация: если >= порога, то 1.0 (белый), иначе 0.0 (черный)
    float segmented = (vesselness >= uThreshold) ? 1.0 : 0.0;
    
    FragColor = vec4(vec3(segmented), 1.0);
}
)";

// Overlay shader - накладывает сегментированную маску на исходное изображение
const char* overlayFragmentSrc = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uOriginal;
uniform sampler2D uSegmented;

void main() {
    vec4 original = texture(uOriginal, vUv);
    // Конвертируем RGB в grayscale, если нужно
    float gray = dot(original.rgb, vec3(0.299, 0.587, 0.114));
    float mask = texture(uSegmented, vUv).x;
    
    // Добавляем бинарную маску к исходному изображению
    vec3 overlay = vec3(gray) + vec3(mask * 0.5);  // Полупрозрачная маска
    
    FragColor = vec4(overlay, 1.0);
}
)";

// ============================================================================
// C++ UTILITY FUNCTIONS
// ============================================================================

// Генерация 1D ядер производных Гауссиана
std::vector<float> generateGaussianKernel(float sigma, int derivative_order) {
    int kernel_size = 2 * std::round(3.0f * sigma) + 1;
    int center = kernel_size / 2;
    std::vector<float> kernel(kernel_size);
    
    float sigma2 = sigma * sigma;
    float sigma4 = sigma2 * sigma2;
    float sigma6 = sigma4 * sigma2;
    
    float norm_factor = 1.0f / std::sqrt(2.0f * M_PI * sigma2);
    
    for (int i = 0; i < kernel_size; i++) {
        float x = static_cast<float>(i - center);
        float gauss = std::exp(-(x * x) / (2.0f * sigma2));
        
        if (derivative_order == 0) {
            // G(x) - обычное Гауссово размытие
            kernel[i] = gauss * norm_factor;
        }
        else if (derivative_order == 1) {
            // dG/dx = -x/σ² * G(x)
            kernel[i] = (-x / sigma2) * gauss * norm_factor;
        }
        else if (derivative_order == 2) {
            // d²G/dx² = (x²/σ⁴ - 1/σ²) * G(x)
            kernel[i] = ((x * x / sigma4) - (1.0f / sigma2)) * gauss * norm_factor;
        }
    }
    
    // Нормализация (для 0-й производной сумма должна быть 1)
    if (derivative_order == 0) {
        float sum = 0.0f;
        for (float val : kernel) {
            sum += val;
        }
        if (sum > 1e-10f) {
            for (float& val : kernel) {
                val /= sum;
            }
        }
    }
    
    return kernel;
}

// ============================================================================
// GLRenderer IMPLEMENTATION
// ============================================================================

GLRenderer::GLRenderer()
    : initialized(false), currentWidth(0), currentHeight(0),
      globalContrastShader(0), grayscaleShader(0), invertShader(0),
      downscaleShader(0), convolve1DShader(0), scaleNormShader(0),
      eigenvaluesShader(0), vesselnessShader(0),
      segmentationShader(0), overlayShader(0),
      fboPreprocessed(0), fboGray(0), fboDownscaled(0), fboInvert(0),
      fboDxxTemp(0), fboDxx(0), fboDyyTemp(0), fboDyy(0), fboDxyTemp(0), fboDxy(0),
      fboHessian(0), fboEigenvalues(0), fboVesselness(0),
      fboSegmentation(0), fboOverlayDownscaled(0), fboOverlay(0),
      texPreprocessed(0), texGray(0), texDownscaled(0), texInvert(0),
      texDxxTemp(0), texDxx(0), texDyyTemp(0), texDyy(0), texDxyTemp(0), texDxy(0),
      texHessian(0), texEigenvalues(0), texVesselness(0),
      texSegmentation(0), texOverlayDownscaled(0), texOverlay(0),
      inputTexture(0), vao(0), vbo(0),
      downscaledWidth(0), downscaledHeight(0), currentDownscaleFactor(1.0f) {
}

GLRenderer::~GLRenderer() {
    cleanup();
}

bool GLRenderer::isGPUAvailable() {
    const char* version = (const char*)glGetString(GL_VERSION);
    if (!version) return false;
    
    int major = 0, minor = 0;
    sscanf(version, "%d.%d", &major, &minor);
    return (major > 3) || (major == 3 && minor >= 3);
}

GLuint GLRenderer::compileShader(const char* vertexSrc, const char* fragmentSrc) {
    // Компиляция вершинного шейдера
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
    glCompileShader(vertexShader);
    
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
        return 0;
    }
    
    // Компиляция фрагментного шейдера
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }
    
    // Линковка программы
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void GLRenderer::createFramebuffer(GLuint* fbo, GLuint* texture, int width, int height) {
    // Создаем текстуру
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Создаем framebuffer
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool GLRenderer::initialize() {
    if (initialized) return true;
    
    std::cout << "Initializing GL Renderer..." << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    // Компилируем все шейдеры
    globalContrastShader = compileShader(vertexShaderSrc, globalContrastFragmentSrc);
    grayscaleShader = compileShader(vertexShaderSrc, grayscaleFragmentSrc);
    downscaleShader = compileShader(vertexShaderSrc, downscaleFragmentSrc);
    invertShader = compileShader(vertexShaderSrc, invertFragmentSrc);
    convolve1DShader = compileShader(vertexShaderSrc, convolve1DFragmentSrc);
    scaleNormShader = compileShader(vertexShaderSrc, scaleNormalizationFragmentSrc);
    eigenvaluesShader = compileShader(vertexShaderSrc, eigenvaluesFragmentSrc);
    vesselnessShader = compileShader(vertexShaderSrc, vesselnessFragmentSrc);
    segmentationShader = compileShader(vertexShaderSrc, segmentationFragmentSrc);
    overlayShader = compileShader(vertexShaderSrc, overlayFragmentSrc);
    
    if (!globalContrastShader || !grayscaleShader || !downscaleShader || !invertShader ||
        !convolve1DShader || !scaleNormShader ||
        !eigenvaluesShader || !vesselnessShader ||
        !segmentationShader || !overlayShader) {
        std::cerr << "Failed to compile shaders!" << std::endl;
        return false;
    }
    
    // Создаем VAO и VBO для quad
    float vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
         1.0f,  1.0f,  1.0f, 1.0f   // top-right
    };
    
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // TexCoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
    
    initialized = true;
    std::cout << "GL Renderer initialized successfully!" << std::endl;
    
    return true;
}

void GLRenderer::recreateFramebuffers(int width, int height, float downscaleFactor) {
    // Удаляем старые framebuffers и текстуры
    if (fboGray) {
        GLuint fbos[] = {
            fboPreprocessed, fboGray, fboDownscaled, fboInvert,
            fboDxxTemp, fboDxx, fboDyyTemp, fboDyy, fboDxyTemp, fboDxy,
            fboHessian, fboEigenvalues, fboVesselness,
            fboSegmentation, fboOverlay
        };
        glDeleteFramebuffers(15, fbos);
        
        GLuint textures[] = {
            texPreprocessed, texGray, texDownscaled, texInvert,
            texDxxTemp, texDxx, texDyyTemp, texDyy, texDxyTemp, texDxy,
            texHessian, texEigenvalues, texVesselness,
            texSegmentation, texOverlay
        };
        glDeleteTextures(15, textures);
    }
    
    // Вычисляем размеры для downscaled изображения
    downscaledWidth = static_cast<int>(std::round(width * downscaleFactor));
    downscaledHeight = static_cast<int>(std::round(height * downscaleFactor));
    // Минимум 1 пиксель
    if (downscaledWidth < 1) downscaledWidth = 1;
    if (downscaledHeight < 1) downscaledHeight = 1;
    
    // Создаем новые
    createFramebuffer(&fboPreprocessed, &texPreprocessed, width, height);
    // Downscaled создается в оригинальном разрешении (RGB), затем уменьшается
    createFramebuffer(&fboDownscaled, &texDownscaled, downscaledWidth, downscaledHeight);
    // Gray создается в downscaled разрешении (grayscale)
    createFramebuffer(&fboGray, &texGray, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboInvert, &texInvert, downscaledWidth, downscaledHeight);
    
    // Temp buffers для вычисления производных (используем downscaled размер)
    createFramebuffer(&fboDxxTemp, &texDxxTemp, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboDxx, &texDxx, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboDyyTemp, &texDyyTemp, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboDyy, &texDyy, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboDxyTemp, &texDxyTemp, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboDxy, &texDxy, downscaledWidth, downscaledHeight);
    
    createFramebuffer(&fboHessian, &texHessian, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboEigenvalues, &texEigenvalues, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboVesselness, &texVesselness, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboSegmentation, &texSegmentation, downscaledWidth, downscaledHeight);
    createFramebuffer(&fboOverlayDownscaled, &texOverlayDownscaled, downscaledWidth, downscaledHeight);  // Overlay в downscaled разрешении
    createFramebuffer(&fboOverlay, &texOverlay, width, height);  // Overlay в оригинальном разрешении (после масштабирования)
    
    currentWidth = width;
    currentHeight = height;
    
    std::cout << "Framebuffers recreated: " << width << "x" << height << std::endl;
}

void GLRenderer::uploadTexture(const cv::Mat& image) {
    // Конвертируем в RGB если нужно
    cv::Mat rgb;
    if (image.channels() == 1) {
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgb, cv::COLOR_BGRA2RGB);
    } else {
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    }
    
    // Создаем или обновляем входную текстуру
    if (inputTexture == 0) {
        glGenTextures(1, &inputTexture);
    }
    
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

cv::Mat GLRenderer::downloadTexture(GLuint texture, int width, int height) {
    // Получаем реальный размер текстуры
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint texWidth, texHeight;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    
    // Используем правильный порядок: Mat(rows, cols, type) и реальные размеры текстуры
    cv::Mat result(texHeight, texWidth, CV_32FC4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, result.data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Конвертируем в 8-bit grayscale для отображения
    cv::Mat gray;
    cv::extractChannel(result, gray, 0);
    gray.convertTo(gray, CV_8U, 255.0);
    
    return gray;
}

void GLRenderer::renderPass(GLuint program, GLuint targetFBO, GLuint inputTex) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, currentWidth, currentHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(program);
    glBindVertexArray(vao);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderer::renderPassToDownscaled(GLuint program, GLuint targetFBO, GLuint inputTex) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, downscaledWidth, downscaledHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(program);
    glBindVertexArray(vao);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLRenderer::setConvolveKernel(const std::vector<float>& kernel, int direction) {
    glUseProgram(convolve1DShader);
    
    // Передаем ядро свертки
    glUniform1fv(glGetUniformLocation(convolve1DShader, "uKernel"), 
                 std::min((int)kernel.size(), 64), kernel.data());
    glUniform1i(glGetUniformLocation(convolve1DShader, "uKernelSize"), kernel.size());
    glUniform1i(glGetUniformLocation(convolve1DShader, "uDirection"), direction);
    
    glUseProgram(0);
}

cv::Mat GLRenderer::processFrame(const cv::Mat& input, float sigma, float beta, float c,
                                  int displayStage, bool invertEnabled,
                                  bool globalContrastEnabled, float brightness, float contrast,
                                  bool claheEnabled, int claheIterations, float claheTarget,
                                  float segmentationThreshold, float downscaleFactor) {
    if (!initialized) {
        std::cerr << "GL Renderer not initialized!" << std::endl;
        return cv::Mat();
    }
    
    // Ограничиваем downscaleFactor в диапазоне [0.25, 1.0] (соответствует divisor 1, 2, 4)
    downscaleFactor = std::max(0.25f, std::min(1.0f, downscaleFactor));
    
    // Пересоздаем framebuffers если размер изменился или изменился downscaleFactor
    if (input.cols != currentWidth || input.rows != currentHeight || 
        downscaleFactor != currentDownscaleFactor) {
        recreateFramebuffers(input.cols, input.rows, downscaleFactor);
        currentDownscaleFactor = downscaleFactor;
    }
    
    // Загружаем входное изображение
    uploadTexture(input);
    
    // Pass -1: Global Contrast (опционально)
    GLuint textureAfterPreprocessing = inputTexture;
    if (globalContrastEnabled) {
        glUseProgram(globalContrastShader);
        glUniform1f(glGetUniformLocation(globalContrastShader, "uBrightness"), brightness);
        glUniform1f(glGetUniformLocation(globalContrastShader, "uContrast"), contrast);
        glUseProgram(0);
        
        renderPass(globalContrastShader, fboPreprocessed, inputTexture);
        textureAfterPreprocessing = texPreprocessed;
    }
    
    // Pass 0: Downscale (уменьшение разрешения) - ПЕРЕД grayscale
    // Всегда рендерим в fboDownscaled, чтобы texDownscaled всегда содержал валидное изображение
    GLuint textureAfterDownscale;
    glBindFramebuffer(GL_FRAMEBUFFER, fboDownscaled);
    glViewport(0, 0, downscaledWidth, downscaledHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(downscaleShader);
    glBindVertexArray(vao);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAfterPreprocessing);
    glUniform1i(glGetUniformLocation(downscaleShader, "uTexture"), 0);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    textureAfterDownscale = texDownscaled;
    
    // Pass 0.5: Grayscale (на downscaled изображении)
    renderPassToDownscaled(grayscaleShader, fboGray, textureAfterDownscale);
    
    // Pass 1: Invert (опционально) - работает с grayscale
    GLuint textureAfterInvert;
    if (invertEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, fboInvert);
        glViewport(0, 0, downscaledWidth, downscaledHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(invertShader);
        glBindVertexArray(vao);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texGray);  // Используем grayscale изображение
        glUniform1i(glGetUniformLocation(invertShader, "uTexture"), 0);
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        glBindVertexArray(0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        textureAfterInvert = texInvert;
    } else {
        textureAfterInvert = texGray;  // Используем grayscale, а не downscaled RGB
    }
    
    // ========================================================================
    // НОВЫЙ ПАЙПЛАЙН: Вычисление Гессиана через производные Гауссиана
    // ========================================================================
    
    // Генерируем ядра производных Гауссиана
    auto kernel_g = generateGaussianKernel(sigma, 0);    // G (0-я производная)
    auto kernel_dg = generateGaussianKernel(sigma, 1);   // dG/dx (1-я производная)
    auto kernel_d2g = generateGaussianKernel(sigma, 2);  // d²G/dx² (2-я производная)
    
    std::cout << "Sigma: " << sigma 
              << ", Kernel sizes: G=" << kernel_g.size() 
              << ", dG=" << kernel_dg.size() 
              << ", d²G=" << kernel_d2g.size() << std::endl;
    
    // --- Вычисление Dxx = (d²G/dx²) ⊗ G ---
    // Проход 1: Horizontal d²G/dx²
    setConvolveKernel(kernel_d2g, 0);  // direction=0 (horizontal)
    renderPassToDownscaled(convolve1DShader, fboDxxTemp, textureAfterInvert);
    
    // Проход 2: Vertical G
    setConvolveKernel(kernel_g, 1);  // direction=1 (vertical)
    renderPassToDownscaled(convolve1DShader, fboDxx, texDxxTemp);
    
    // --- Вычисление Dyy = G ⊗ (d²G/dy²) ---
    // Проход 1: Horizontal G
    setConvolveKernel(kernel_g, 0);
    renderPassToDownscaled(convolve1DShader, fboDyyTemp, textureAfterInvert);
    
    // Проход 2: Vertical d²G/dy²
    setConvolveKernel(kernel_d2g, 1);
    renderPassToDownscaled(convolve1DShader, fboDyy, texDyyTemp);
    
    // --- Вычисление Dxy = (dG/dx) ⊗ (dG/dy) ---
    // Проход 1: Horizontal dG/dx
    setConvolveKernel(kernel_dg, 0);
    renderPassToDownscaled(convolve1DShader, fboDxyTemp, textureAfterInvert);
    
    // Проход 2: Vertical dG/dy
    setConvolveKernel(kernel_dg, 1);
    renderPassToDownscaled(convolve1DShader, fboDxy, texDxyTemp);
    
    // --- Scale Normalization: умножаем на σ² ---
    glBindFramebuffer(GL_FRAMEBUFFER, fboHessian);
    glViewport(0, 0, downscaledWidth, downscaledHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(scaleNormShader);
    glUniform1i(glGetUniformLocation(scaleNormShader, "uDxx"), 0);
    glUniform1i(glGetUniformLocation(scaleNormShader, "uDyy"), 1);
    glUniform1i(glGetUniformLocation(scaleNormShader, "uDxy"), 2);
    glUniform1f(glGetUniformLocation(scaleNormShader, "uSigma"), sigma);
    
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texDxx);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texDyy);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texDxy);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // ========================================================================
    // Pass: Eigenvalues
    // ========================================================================
    renderPassToDownscaled(eigenvaluesShader, fboEigenvalues, texHessian);
    
    // ========================================================================
    // Pass: Vesselness
    // ========================================================================
    glUseProgram(vesselnessShader);
    glUniform1f(glGetUniformLocation(vesselnessShader, "uBeta"), beta);
    glUniform1f(glGetUniformLocation(vesselnessShader, "uC"), c);
    glUniform1i(glGetUniformLocation(vesselnessShader, "uBlackWhite"), invertEnabled ? 0 : 1);
    glUseProgram(0);
    
    renderPassToDownscaled(vesselnessShader, fboVesselness, texEigenvalues);
    
    // ========================================================================
    // Pass: Segmentation
    // ========================================================================
    glBindFramebuffer(GL_FRAMEBUFFER, fboSegmentation);
    glViewport(0, 0, downscaledWidth, downscaledHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(segmentationShader);
    glUniform1f(glGetUniformLocation(segmentationShader, "uThreshold"), segmentationThreshold);
    
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texVesselness);
    glUniform1i(glGetUniformLocation(segmentationShader, "uTexture"), 0);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // ========================================================================
    // Pass: Overlay (сначала в downscaled разрешении, затем масштабируем обратно)
    // ========================================================================
    
    // Шаг 1: Рендерим overlay в downscaled разрешении
    glBindFramebuffer(GL_FRAMEBUFFER, fboOverlayDownscaled);
    glViewport(0, 0, downscaledWidth, downscaledHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(overlayShader);
    glBindVertexArray(vao);
    
    glActiveTexture(GL_TEXTURE0);
    // Используем downscaled RGB изображение - шейдер сам конвертирует в grayscale
    glBindTexture(GL_TEXTURE_2D, texDownscaled);
    glUniform1i(glGetUniformLocation(overlayShader, "uOriginal"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texSegmentation);
    glUniform1i(glGetUniformLocation(overlayShader, "uSegmented"), 1);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Шаг 2: Масштабируем overlay обратно в оригинальное разрешение
    glBindFramebuffer(GL_FRAMEBUFFER, fboOverlay);
    glViewport(0, 0, currentWidth, currentHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(downscaleShader);  // Используем downscale shader для upscaling (билинейная интерполяция)
    glBindVertexArray(vao);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texOverlayDownscaled);
    // Устанавливаем параметры текстуры для правильного масштабирования
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(glGetUniformLocation(downscaleShader, "uTexture"), 0);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // ========================================================================
    // Выбираем какую текстуру вернуть
    // ========================================================================
    GLuint textureToShow;
    int textureWidth, textureHeight;
    switch(displayStage) {
        case 0: 
            textureToShow = texGray; 
            textureWidth = downscaledWidth;  // Grayscale теперь в downscaled разрешении
            textureHeight = downscaledHeight;
            break;
        case 1: 
            textureToShow = texInvert; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;
        case 2: 
            textureToShow = texDxx; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;  // Dxx для отладки
        case 3: 
            textureToShow = texHessian; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;
        case 4: 
            textureToShow = texEigenvalues; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;
        case 5: 
            textureToShow = texVesselness; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;
        case 6: 
            textureToShow = texSegmentation; 
            textureWidth = downscaledWidth;
            textureHeight = downscaledHeight;
            break;
        case 7:
        case 8:
        default: 
            textureToShow = texOverlay; 
            textureWidth = currentWidth;
            textureHeight = currentHeight;
            break;
    }
    
    return downloadTexture(textureToShow, textureWidth, textureHeight);
}

void GLRenderer::cleanup() {
    if (!initialized) return;
    
    // Удаляем шейдеры
    if (globalContrastShader) glDeleteProgram(globalContrastShader);
    if (grayscaleShader) glDeleteProgram(grayscaleShader);
    if (downscaleShader) glDeleteProgram(downscaleShader);
    if (invertShader) glDeleteProgram(invertShader);
    if (convolve1DShader) glDeleteProgram(convolve1DShader);
    if (scaleNormShader) glDeleteProgram(scaleNormShader);
    if (eigenvaluesShader) glDeleteProgram(eigenvaluesShader);
    if (vesselnessShader) glDeleteProgram(vesselnessShader);
    if (segmentationShader) glDeleteProgram(segmentationShader);
    if (overlayShader) glDeleteProgram(overlayShader);
    
    // Удаляем framebuffers и текстуры
    if (fboGray) {
        GLuint fbos[] = {
            fboPreprocessed, fboGray, fboDownscaled, fboInvert,
            fboDxxTemp, fboDxx, fboDyyTemp, fboDyy, fboDxyTemp, fboDxy,
            fboHessian, fboEigenvalues, fboVesselness,
            fboSegmentation, fboOverlayDownscaled, fboOverlay
        };
        glDeleteFramebuffers(16, fbos);
        
        GLuint textures[] = {
            texPreprocessed, texGray, texDownscaled, texInvert,
            texDxxTemp, texDxx, texDyyTemp, texDyy, texDxyTemp, texDxy,
            texHessian, texEigenvalues, texVesselness,
            texSegmentation, texOverlayDownscaled, texOverlay
        };
        glDeleteTextures(16, textures);
    }
    
    if (inputTexture) glDeleteTextures(1, &inputTexture);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    
    initialized = false;
    std::cout << "GL Renderer cleaned up" << std::endl;
}