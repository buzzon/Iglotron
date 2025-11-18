#include "frangiglwidget.h"
#include <QOpenGLBuffer>
#include <QDebug>

FrangiGLWidget::FrangiGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_grayscaleShader(nullptr)
    , m_invertShader(nullptr)
    , m_blurXShader(nullptr)
    , m_blurYShader(nullptr)
    , m_gradientsShader(nullptr)
    , m_hessianShader(nullptr)
    , m_eigenvaluesShader(nullptr)
    , m_vesselnessShader(nullptr)
    , m_visualizeShader(nullptr)
    , m_fboGray(nullptr)
    , m_fboInvert(nullptr)
    , m_fboBlurX(nullptr)
    , m_fboBlurY(nullptr)
    , m_fboGradients(nullptr)
    , m_fboHessian(nullptr)
    , m_fboEigenvalues(nullptr)
    , m_fboVesselness(nullptr)
    , m_inputTexture(nullptr)
    , m_sigma(1.5f)
    , m_beta(0.5f)
    , m_c(15.0f)
    , m_displayStage(6)  // По умолчанию показываем vesselness (теперь stage 6)
    , m_invertEnabled(true)  // По умолчанию инверсия включена
    , m_vao(nullptr)
    , m_vbo(0)
{
}

FrangiGLWidget::~FrangiGLWidget()
{
    makeCurrent();
    
    delete m_grayscaleShader;
    delete m_invertShader;
    delete m_blurXShader;
    delete m_blurYShader;
    delete m_gradientsShader;
    delete m_hessianShader;
    delete m_eigenvaluesShader;
    delete m_vesselnessShader;
    delete m_visualizeShader;
    
    delete m_fboGray;
    delete m_fboInvert;
    delete m_fboBlurX;
    delete m_fboBlurY;
    delete m_fboGradients;
    delete m_fboHessian;
    delete m_fboEigenvalues;
    delete m_fboVesselness;
    
    delete m_inputTexture;
    delete m_vao;
    
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
    }
    
    doneCurrent();
}

void FrangiGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    qDebug() << "OpenGL initialized";
    qDebug() << "OpenGL version:" << (const char*)glGetString(GL_VERSION);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Создаем VAO и VBO для quad
    m_vao = new QOpenGLVertexArrayObject(this);
    m_vao->create();
    m_vao->bind();
    
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    m_vao->release();
    
    createShaders();
    createFramebuffers();
}

void FrangiGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void FrangiGLWidget::paintGL()
{
    if (!m_inputTexture || m_currentFrame.isNull()) {
        glClear(GL_COLOR_BUFFER_BIT);
        qDebug() << "paintGL: No texture or frame";
        return;
    }
    
    qDebug() << "paintGL: Processing frame" << m_currentFrame.size();
    processFrame();
}

void FrangiGLWidget::setFrame(const QImage &frame)
{
    if (frame.isNull()) {
        qDebug() << "Frame is null!";
        return;
    }
    
    m_currentFrame = frame.convertToFormat(QImage::Format_RGB888);
    
    makeCurrent();
    
    // Пересоздаем framebuffer'ы если размер изображения изменился
    if (!m_fboGray ||
        m_fboGray->width() != m_currentFrame.width() ||
        m_fboGray->height() != m_currentFrame.height()) {
        recreateFramebuffers(m_currentFrame.width(), m_currentFrame.height());
    }
    
    if (m_inputTexture) {
        delete m_inputTexture;
    }
    
    m_inputTexture = new QOpenGLTexture(m_currentFrame.mirrored());
    m_inputTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_inputTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_inputTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    
    doneCurrent();
    update();
}

void FrangiGLWidget::createShaders()
{
    QString vertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec2 texCoord;
        out vec2 vUv;
        
        void main() {
            vUv = texCoord;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";
    
    // Grayscale shader
    m_grayscaleShader = new QOpenGLShaderProgram(this);
    m_grayscaleShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_grayscaleShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
        #version 330 core
        in vec2 vUv;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        
        void main() {
            vec4 color = texture(uTexture, vUv);
            float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
            FragColor = vec4(gray, gray, gray, 1.0);
        }
    )");
    if (!m_grayscaleShader->link()) {
        qDebug() << "Grayscale shader link error:" << m_grayscaleShader->log();
    } else {
        qDebug() << "Grayscale shader linked successfully";
    }
    
    // Invert shader
    m_invertShader = new QOpenGLShaderProgram(this);
    m_invertShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_invertShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
        #version 330 core
        in vec2 vUv;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        
        void main() {
            vec4 color = texture(uTexture, vUv);
            float inverted = 1.0 - color.x;
            FragColor = vec4(inverted, inverted, inverted, 1.0);
        }
    )");
    m_invertShader->link();
    
    // Blur X shader
    m_blurXShader = new QOpenGLShaderProgram(this);
    m_blurXShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_blurXShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_blurXShader->link();
    
    // Blur Y shader
    m_blurYShader = new QOpenGLShaderProgram(this);
    m_blurYShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_blurYShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_blurYShader->link();
    
    // Gradients shader (Sobel)
    m_gradientsShader = new QOpenGLShaderProgram(this);
    m_gradientsShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_gradientsShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_gradientsShader->link();
    
    // Hessian shader
    m_hessianShader = new QOpenGLShaderProgram(this);
    m_hessianShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_hessianShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_hessianShader->link();
    
    // Eigenvalues shader
    m_eigenvaluesShader = new QOpenGLShaderProgram(this);
    m_eigenvaluesShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_eigenvaluesShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_eigenvaluesShader->link();
    
    // Vesselness shader
    m_vesselnessShader = new QOpenGLShaderProgram(this);
    m_vesselnessShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_vesselnessShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
    )");
    m_vesselnessShader->link();
    
    // Visualize shader
    m_visualizeShader = new QOpenGLShaderProgram(this);
    m_visualizeShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader);
    m_visualizeShader->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
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
            } else {
                // Grayscale, Invert, Blur: обычная визуализация
                float v = texel.x;
                v = clamp(v, 0.0, 1.0);
                color = vec3(v);
            }
            
            FragColor = vec4(color, 1.0);
        }
    )");
    m_visualizeShader->link();
}

void FrangiGLWidget::createFramebuffers()
{
    // Framebuffers будут созданы динамически в setFrame()
}

void FrangiGLWidget::recreateFramebuffers(int width, int height)
{
    // Удаляем старые framebuffer'ы
    delete m_fboGray;
    delete m_fboInvert;
    delete m_fboBlurX;
    delete m_fboBlurY;
    delete m_fboGradients;
    delete m_fboHessian;
    delete m_fboEigenvalues;
    delete m_fboVesselness;
    
    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA32F);
    format.setTextureTarget(GL_TEXTURE_2D);
    
    m_fboGray = new QOpenGLFramebufferObject(width, height, format);
    m_fboInvert = new QOpenGLFramebufferObject(width, height, format);
    m_fboBlurX = new QOpenGLFramebufferObject(width, height, format);
    m_fboBlurY = new QOpenGLFramebufferObject(width, height, format);
    m_fboGradients = new QOpenGLFramebufferObject(width, height, format);
    m_fboHessian = new QOpenGLFramebufferObject(width, height, format);
    m_fboEigenvalues = new QOpenGLFramebufferObject(width, height, format);
    m_fboVesselness = new QOpenGLFramebufferObject(width, height, format);
    
    qDebug() << "Framebuffers recreated with size:" << width << "x" << height;
}

void FrangiGLWidget::renderPass(QOpenGLShaderProgram *program, 
                                 QOpenGLFramebufferObject *target,
                                 QOpenGLTexture *inputTexture)
{
    if (target) {
        target->bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    }
    
    program->bind();
    m_vao->bind();
    
    if (inputTexture) {
        inputTexture->bind(0);
        program->setUniformValue("uTexture", 0);
    }
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    m_vao->release();
    program->release();
    
    if (target) {
        target->release();
    }
}

void FrangiGLWidget::processFrame()
{
    if (!m_fboGray) return;
    
    int w = m_fboGray->width();
    int h = m_fboGray->height();
    
    // Pass 0: Grayscale
    m_fboGray->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_grayscaleShader->bind();
    m_vao->bind();
    m_inputTexture->bind(0);
    m_grayscaleShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_grayscaleShader->release();
    m_fboGray->release();
    
    // Pass 1: Invert (опционально)
    GLuint textureAfterInvert;
    if (m_invertEnabled) {
        m_fboInvert->bind();
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        m_invertShader->bind();
        m_vao->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fboGray->texture());
        m_invertShader->setUniformValue("uTexture", 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_vao->release();
        m_invertShader->release();
        m_fboInvert->release();
        textureAfterInvert = m_fboInvert->texture();
    } else {
        // Пропускаем инверсию, используем grayscale напрямую
        textureAfterInvert = m_fboGray->texture();
    }
    
    // Pass 2: Blur X
    m_fboBlurX->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurXShader->bind();
    m_blurXShader->setUniformValue("uSigma", m_sigma);
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureAfterInvert);
    m_blurXShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_blurXShader->release();
    m_fboBlurX->release();
    
    // Pass 3: Blur Y
    m_fboBlurY->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_blurYShader->bind();
    m_blurYShader->setUniformValue("uSigma", m_sigma);
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboBlurX->texture());
    m_blurYShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_blurYShader->release();
    m_fboBlurY->release();
    
    // Pass 4: Gradients
    m_fboGradients->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_gradientsShader->bind();
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboBlurY->texture());
    m_gradientsShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_gradientsShader->release();
    m_fboGradients->release();
    
    // Pass 5: Hessian
    m_fboHessian->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_hessianShader->bind();
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboGradients->texture());
    m_hessianShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_hessianShader->release();
    m_fboHessian->release();
    
    // Pass 6: Eigenvalues
    m_fboEigenvalues->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_eigenvaluesShader->bind();
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboHessian->texture());
    m_eigenvaluesShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_eigenvaluesShader->release();
    m_fboEigenvalues->release();
    
    // Pass 7: Vesselness
    m_fboVesselness->bind();
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    m_vesselnessShader->bind();
    m_vesselnessShader->setUniformValue("uBeta", m_beta);
    m_vesselnessShader->setUniformValue("uC", m_c);
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fboEigenvalues->texture());
    m_vesselnessShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_vesselnessShader->release();
    m_fboVesselness->release();
    
    // Pass 8: Final visualization to screen
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, width(), height());
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Выбираем какую текстуру показывать в зависимости от m_displayStage
    GLuint textureToShow;
    switch(m_displayStage) {
        case 0: textureToShow = m_fboGray->texture(); break;
        case 1: textureToShow = m_fboInvert->texture(); break;
        case 2: textureToShow = m_fboBlurY->texture(); break;
        case 3: textureToShow = m_fboGradients->texture(); break;
        case 4: textureToShow = m_fboHessian->texture(); break;
        case 5: textureToShow = m_fboEigenvalues->texture(); break;
        case 6:
        default: textureToShow = m_fboVesselness->texture(); break;
    }
    
    m_visualizeShader->bind();
    m_visualizeShader->setUniformValue("uStage", m_displayStage);
    m_vao->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureToShow);
    m_visualizeShader->setUniformValue("uTexture", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao->release();
    m_visualizeShader->release();
}