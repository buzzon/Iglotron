#include "gl_renderer.h"
#include <iostream>

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

// Blur X shader
const char* blurXFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform float uSigma;

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    float h = 1.0 / texSize.x;
    vec4 sum = vec4(0.0);
    float totalWeight = 0.0;
    
    for(int i = -15; i <= 15; i++) {
        float offset = float(i) * h;
        float weight = exp(-float(i*i) / (2.0 * uSigma * uSigma));
        sum += texture(uTexture, vUv + vec2(offset, 0.0)) * weight;
        totalWeight += weight;
    }
    
    FragColor = sum / totalWeight;
}
)";

// Blur Y shader
const char* blurYFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform float uSigma;

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    float h = 1.0 / texSize.y;
    vec4 sum = vec4(0.0);
    float totalWeight = 0.0;
    
    for(int i = -15; i <= 15; i++) {
        float offset = float(i) * h;
        float weight = exp(-float(i*i) / (2.0 * uSigma * uSigma));
        sum += texture(uTexture, vUv + vec2(0.0, offset)) * weight;
        totalWeight += weight;
    }
    
    FragColor = sum / totalWeight;
}
)";

// Gradients shader (Sobel)
const char* gradientsFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    float h = 1.0 / texSize.x;
    
    // Sobel X
    float gx = texture(uTexture, vUv + vec2(-h, -h)).x * -1.0;
    gx += texture(uTexture, vUv + vec2(-h, 0.0)).x * -2.0;
    gx += texture(uTexture, vUv + vec2(-h, h)).x * -1.0;
    gx += texture(uTexture, vUv + vec2(h, -h)).x * 1.0;
    gx += texture(uTexture, vUv + vec2(h, 0.0)).x * 2.0;
    gx += texture(uTexture, vUv + vec2(h, h)).x * 1.0;
    gx /= 8.0;
    
    // Sobel Y
    float gy = texture(uTexture, vUv + vec2(-h, -h)).x * -1.0;
    gy += texture(uTexture, vUv + vec2(0.0, -h)).x * -2.0;
    gy += texture(uTexture, vUv + vec2(h, -h)).x * -1.0;
    gy += texture(uTexture, vUv + vec2(-h, h)).x * 1.0;
    gy += texture(uTexture, vUv + vec2(0.0, h)).x * 2.0;
    gy += texture(uTexture, vUv + vec2(h, h)).x * 1.0;
    gy /= 8.0;
    
    FragColor = vec4(gx, gy, 0.0, 1.0);
}
)";

// Hessian shader
const char* hessianFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    vec2 texSize = vec2(textureSize(uTexture, 0));
    float h = 2.0 / texSize.x;
    
    vec4 c = texture(uTexture, vUv);
    vec4 px = texture(uTexture, vUv + vec2(h, 0.0));
    vec4 nx = texture(uTexture, vUv - vec2(h, 0.0));
    vec4 py = texture(uTexture, vUv + vec2(0.0, h));
    vec4 ny = texture(uTexture, vUv - vec2(0.0, h));
    
    float fxx = (px.x - nx.x) / (2.0 * h);
    float fyy = (py.y - ny.y) / (2.0 * h);
    float fxy = (py.x - ny.x) / (2.0 * h);
    
    FragColor = vec4(fxx, fxy, fyy, 1.0);
}
)";

// Eigenvalues shader
const char* eigenvaluesFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    vec4 h = texture(uTexture, vUv);
    float fxx = h.x;
    float fxy = h.y;
    float fyy = h.z;
    
    float trace = fxx + fyy;
    float det = fxx * fyy - fxy * fxy;
    
    float disc = trace * trace - 4.0 * det;
    if(disc < 0.0) disc = 0.0;
    
    float sqrtDisc = sqrt(disc);
    float lambda1 = 0.5 * (trace + sqrtDisc);
    float lambda2 = 0.5 * (trace - sqrtDisc);
    
    if(abs(lambda1) > abs(lambda2)) {
        float tmp = lambda1;
        lambda1 = lambda2;
        lambda2 = tmp;
    }
    
    FragColor = vec4(lambda1, lambda2, 0.0, 1.0);
}
)";

// Vesselness shader
const char* vesselnessFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform float uBeta;
uniform float uC;

void main() {
    vec4 eigs = texture(uTexture, vUv);
    float lambda1 = eigs.x;
    float lambda2 = eigs.y;
    
    float vesselness = 0.0;
    
    if(lambda2 < 0.0) {
        float beta_sq = uBeta * uBeta;
        float c_sq = uC * uC;
        
        float rb = lambda1 / (lambda2 + 1e-6);
        rb = rb * rb;
        
        float s2 = lambda1*lambda1 + lambda2*lambda2;
        
        float term1 = exp(-rb / beta_sq);
        float term2 = 1.0 - exp(-s2 / c_sq);
        
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
    float mask = texture(uSegmented, vUv).x;
    
    // Просто добавляем бинарную маску к исходному изображению
    // mask уже либо 0.0 либо 1.0
    vec3 overlay = original.rgb + vec3(mask);
    
    FragColor = vec4(overlay, 1.0);
}
)";

// Visualize shader
const char* visualizeFragmentSrc = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform int uStage;

void main() {
    vec4 texel = texture(uTexture, vUv);
    vec3 color;
    
    if(uStage == 3) {
        // Gradients: показываем magnitude градиентов с ОЧЕНЬ сильным усилением
        float gx = texel.x;
        float gy = texel.y;
        float magnitude = sqrt(gx*gx + gy*gy);
        magnitude = magnitude * 50.0; // Усиление x50!
        magnitude = clamp(magnitude, 0.0, 1.0);
        // Визуализируем gx как красный, gy как зеленый для отладки
        color = vec3(abs(gx)*50.0, abs(gy)*50.0, magnitude);
        color = clamp(color, 0.0, 1.0);
    } else if(uStage == 4 || uStage == 5) {
        // Hessian/Eigenvalues: могут быть отрицательными, показываем abs
        float v = abs(texel.x);
        v = v * 10.0; // Усиление
        v = clamp(v, 0.0, 1.0);
        color = vec3(v);
    } else if(uStage == 6) {
        // Vesselness: нужно сильное усиление т.к. значения очень маленькие
        float v = texel.x;
        v = v * 100.0; // Усиление x100!
        v = clamp(v, 0.0, 1.0);
        v = v * v; // Мягкий контраст для лучшей видимости
        color = vec3(v);
    } else if(uStage == 7) {
        // Segmentation: бинарная маска, показываем как есть
        float v = texel.x;
        color = vec3(v);
    } else if(uStage == 8) {
        // Overlay: уже цветное, просто показываем как есть
        color = texel.rgb;
    } else {
        // Grayscale, Invert, Blur: обычная визуализация
        float v = texel.x;
        v = clamp(v, 0.0, 1.0);
        color = vec3(v);
    }
    
    FragColor = vec4(color, 1.0);
}
)";

GLRenderer::GLRenderer() 
    : initialized(false), currentWidth(0), currentHeight(0),
      globalContrastShader(0), grayscaleShader(0), invertShader(0), blurXShader(0), blurYShader(0),
      gradientsShader(0), hessianShader(0), eigenvaluesShader(0), 
      vesselnessShader(0), segmentationShader(0), overlayShader(0), visualizeShader(0),
      fboPreprocessed(0), fboGray(0), fboInvert(0), fboBlurX(0), fboBlurY(0),
      fboGradients(0), fboHessian(0), fboEigenvalues(0), fboVesselness(0), fboSegmentation(0), fboOverlay(0),
      texPreprocessed(0), texGray(0), texInvert(0), texBlurX(0), texBlurY(0),
      texGradients(0), texHessian(0), texEigenvalues(0), texVesselness(0), texSegmentation(0), texOverlay(0),
      inputTexture(0), vao(0), vbo(0) {
}

GLRenderer::~GLRenderer() {
    cleanup();
}

bool GLRenderer::isGPUAvailable() {
    // Проверяем версию OpenGL
    const char* version = (const char*)glGetString(GL_VERSION);
    if (!version) return false;
    
    // Проверяем что версия >= 3.3
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
    invertShader = compileShader(vertexShaderSrc, invertFragmentSrc);
    blurXShader = compileShader(vertexShaderSrc, blurXFragmentSrc);
    blurYShader = compileShader(vertexShaderSrc, blurYFragmentSrc);
    gradientsShader = compileShader(vertexShaderSrc, gradientsFragmentSrc);
    hessianShader = compileShader(vertexShaderSrc, hessianFragmentSrc);
    eigenvaluesShader = compileShader(vertexShaderSrc, eigenvaluesFragmentSrc);
    vesselnessShader = compileShader(vertexShaderSrc, vesselnessFragmentSrc);
    segmentationShader = compileShader(vertexShaderSrc, segmentationFragmentSrc);
    overlayShader = compileShader(vertexShaderSrc, overlayFragmentSrc);
    visualizeShader = compileShader(vertexShaderSrc, visualizeFragmentSrc);
    
    if (!globalContrastShader || !grayscaleShader || !invertShader || !blurXShader || !blurYShader ||
        !gradientsShader || !hessianShader || !eigenvaluesShader || 
        !vesselnessShader || !segmentationShader || !overlayShader || !visualizeShader) {
        std::cerr << "Failed to compile shaders!" << std::endl;
        return false;
    }
    
    // Создаем VAO и VBO для quad
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
        -1.0f,  1.0f, 0.0f, 1.0f,  // top-left
         1.0f,  1.0f, 1.0f, 1.0f   // top-right
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

void GLRenderer::recreateFramebuffers(int width, int height) {
    // Удаляем старые framebuffers и текстуры
    if (fboGray) {
        GLuint fbos[] = {fboPreprocessed, fboGray, fboInvert, fboBlurX, fboBlurY, 
                         fboGradients, fboHessian, fboEigenvalues, fboVesselness, fboSegmentation, fboOverlay};
        glDeleteFramebuffers(11, fbos);
        
        GLuint textures[] = {texPreprocessed, texGray, texInvert, texBlurX, texBlurY,
                             texGradients, texHessian, texEigenvalues, texVesselness, texSegmentation, texOverlay};
        glDeleteTextures(11, textures);
    }
    
    // Создаем новые
    createFramebuffer(&fboPreprocessed, &texPreprocessed, width, height);
    createFramebuffer(&fboGray, &texGray, width, height);
    createFramebuffer(&fboInvert, &texInvert, width, height);
    createFramebuffer(&fboBlurX, &texBlurX, width, height);
    createFramebuffer(&fboBlurY, &texBlurY, width, height);
    createFramebuffer(&fboGradients, &texGradients, width, height);
    createFramebuffer(&fboHessian, &texHessian, width, height);
    createFramebuffer(&fboEigenvalues, &texEigenvalues, width, height);
    createFramebuffer(&fboVesselness, &texVesselness, width, height);
    createFramebuffer(&fboSegmentation, &texSegmentation, width, height);
    createFramebuffer(&fboOverlay, &texOverlay, width, height);
    
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
    cv::Mat result(height, width, CV_32FC4);
    
    glBindTexture(GL_TEXTURE_2D, texture);
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

cv::Mat GLRenderer::processFrame(const cv::Mat& input, float sigma, float beta, float c,
                                  int displayStage, bool invertEnabled,
                                  bool globalContrastEnabled, float brightness, float contrast,
                                  bool claheEnabled, int claheIterations, float claheTarget,
                                  float segmentationThreshold) {
    if (!initialized) {
        std::cerr << "GL Renderer not initialized!" << std::endl;
        return cv::Mat();
    }
    
    // Пересоздаем framebuffers если размер изменился
    if (input.cols != currentWidth || input.rows != currentHeight) {
        recreateFramebuffers(input.cols, input.rows);
    }
    
    // Загружаем входное изображение
    uploadTexture(input);
    
    // Pass -1: Global Contrast (опционально, препроцессинг через GPU)
    GLuint textureAfterPreprocessing = inputTexture;
    
    if (globalContrastEnabled) {
        glUseProgram(globalContrastShader);
        glUniform1f(glGetUniformLocation(globalContrastShader, "uBrightness"), brightness);
        glUniform1f(glGetUniformLocation(globalContrastShader, "uContrast"), contrast);
        glUseProgram(0);
        renderPass(globalContrastShader, fboPreprocessed, inputTexture);
        textureAfterPreprocessing = texPreprocessed;
    }
    
    // Примечание: CLAHE применяется на CPU в frangi_processor перед вызовом этой функции
    
    // Pass 0: Grayscale
    renderPass(grayscaleShader, fboGray, textureAfterPreprocessing);
    
    // Pass 1: Invert (опционально)
    GLuint textureAfterInvert;
    if (invertEnabled) {
        renderPass(invertShader, fboInvert, texGray);
        textureAfterInvert = texInvert;
    } else {
        textureAfterInvert = texGray;
    }
    
    // Pass 2: Blur X
    glUseProgram(blurXShader);
    glUniform1f(glGetUniformLocation(blurXShader, "uSigma"), sigma);
    glUseProgram(0);
    renderPass(blurXShader, fboBlurX, textureAfterInvert);
    
    // Pass 3: Blur Y
    glUseProgram(blurYShader);
    glUniform1f(glGetUniformLocation(blurYShader, "uSigma"), sigma);
    glUseProgram(0);
    renderPass(blurYShader, fboBlurY, texBlurX);
    
    // Pass 4: Gradients
    renderPass(gradientsShader, fboGradients, texBlurY);
    
    // Pass 5: Hessian
    renderPass(hessianShader, fboHessian, texGradients);
    
    // Pass 6: Eigenvalues
    renderPass(eigenvaluesShader, fboEigenvalues, texHessian);
    
    // Pass 7: Vesselness
    glUseProgram(vesselnessShader);
    glUniform1f(glGetUniformLocation(vesselnessShader, "uBeta"), beta);
    glUniform1f(glGetUniformLocation(vesselnessShader, "uC"), c);
    glUseProgram(0);
    renderPass(vesselnessShader, fboVesselness, texEigenvalues);
    
    // Pass 8: Segmentation - бинаризация по порогу
    glBindFramebuffer(GL_FRAMEBUFFER, fboSegmentation);
    glViewport(0, 0, currentWidth, currentHeight);
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
    
    // Pass 9: Overlay - накладываем сегментированную маску на оригинал
    glBindFramebuffer(GL_FRAMEBUFFER, fboOverlay);
    glViewport(0, 0, currentWidth, currentHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(overlayShader);
    glBindVertexArray(vao);
    
    // Привязываем оригинальное изображение к текстуре 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glUniform1i(glGetUniformLocation(overlayShader, "uOriginal"), 0);
    
    // Привязываем сегментированную маску к текстуре 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texSegmentation);
    glUniform1i(glGetUniformLocation(overlayShader, "uSegmented"), 1);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Выбираем какую текстуру вернуть
    GLuint textureToShow;
    switch(displayStage) {
        case 0: textureToShow = texGray; break;
        case 1: textureToShow = texInvert; break;
        case 2: textureToShow = texBlurY; break;
        case 3: textureToShow = texGradients; break;
        case 4: textureToShow = texHessian; break;
        case 5: textureToShow = texEigenvalues; break;
        case 6: textureToShow = texVesselness; break;
        case 7: textureToShow = texSegmentation; break;
        case 8:
        default: textureToShow = texOverlay; break;
    }
    
    // Применяем визуализацию (для усиления контраста)
    glBindFramebuffer(GL_FRAMEBUFFER, fboVesselness + 100); // Временный FBO для финального результата
    // На самом деле просто скачиваем текстуру и применяем визуализацию на CPU для простоты
    
    return downloadTexture(textureToShow, currentWidth, currentHeight);
}

void GLRenderer::cleanup() {
    if (!initialized) return;
    
    // Удаляем шейдеры
    if (globalContrastShader) glDeleteProgram(globalContrastShader);
    if (grayscaleShader) glDeleteProgram(grayscaleShader);
    if (invertShader) glDeleteProgram(invertShader);
    if (blurXShader) glDeleteProgram(blurXShader);
    if (blurYShader) glDeleteProgram(blurYShader);
    if (gradientsShader) glDeleteProgram(gradientsShader);
    if (hessianShader) glDeleteProgram(hessianShader);
    if (eigenvaluesShader) glDeleteProgram(eigenvaluesShader);
    if (vesselnessShader) glDeleteProgram(vesselnessShader);
    if (segmentationShader) glDeleteProgram(segmentationShader);
    if (overlayShader) glDeleteProgram(overlayShader);
    if (visualizeShader) glDeleteProgram(visualizeShader);
    
    // Удаляем framebuffers и текстуры
    if (fboGray) {
        GLuint fbos[] = {fboPreprocessed, fboGray, fboInvert, fboBlurX, fboBlurY,
                         fboGradients, fboHessian, fboEigenvalues, fboVesselness, fboSegmentation, fboOverlay};
        glDeleteFramebuffers(11, fbos);
        
        GLuint textures[] = {texPreprocessed, texGray, texInvert, texBlurX, texBlurY,
                             texGradients, texHessian, texEigenvalues, texVesselness, texSegmentation, texOverlay};
        glDeleteTextures(11, textures);
    }
    
    if (inputTexture) glDeleteTextures(1, &inputTexture);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    
    initialized = false;
    std::cout << "GL Renderer cleaned up" << std::endl;
}

