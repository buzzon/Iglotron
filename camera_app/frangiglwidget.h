#ifndef FRANGIGLWIDGET_H
#define FRANGIGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QImage>

class FrangiGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT

public:
    explicit FrangiGLWidget(QWidget *parent = nullptr);
    ~FrangiGLWidget();

    void setFrame(const QImage &frame);
    
    // Параметры Frangi фильтра
    void setSigma(float sigma) { m_sigma = sigma; update(); }
    void setBeta(float beta) { m_beta = beta; update(); }
    void setC(float c) { m_c = c; update(); }
    
    // Выбор отображаемого stage
    void setDisplayStage(int stage) { m_displayStage = stage; update(); }
    
    // Включить/выключить инверсию
    void setInvertEnabled(bool enabled) { m_invertEnabled = enabled; update(); }
    
    // Размер изображения для шейдеров
    int getImageWidth() const { return m_fboGray ? m_fboGray->width() : 512; }
    int getImageHeight() const { return m_fboGray ? m_fboGray->height() : 512; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void createShaders();
    void createFramebuffers();
    void recreateFramebuffers(int width, int height);
    void renderPass(QOpenGLShaderProgram *program, QOpenGLFramebufferObject *target,
                    QOpenGLTexture *inputTexture);
    void processFrame();

    // Шейдерные программы
    QOpenGLShaderProgram *m_grayscaleShader;
    QOpenGLShaderProgram *m_invertShader;
    QOpenGLShaderProgram *m_blurXShader;
    QOpenGLShaderProgram *m_blurYShader;
    QOpenGLShaderProgram *m_gradientsShader;
    QOpenGLShaderProgram *m_hessianShader;
    QOpenGLShaderProgram *m_eigenvaluesShader;
    QOpenGLShaderProgram *m_vesselnessShader;
    QOpenGLShaderProgram *m_overlayShader;
    QOpenGLShaderProgram *m_visualizeShader;

    // Framebuffers для промежуточных результатов
    QOpenGLFramebufferObject *m_fboGray;
    QOpenGLFramebufferObject *m_fboInvert;
    QOpenGLFramebufferObject *m_fboBlurX;
    QOpenGLFramebufferObject *m_fboBlurY;
    QOpenGLFramebufferObject *m_fboGradients;
    QOpenGLFramebufferObject *m_fboHessian;
    QOpenGLFramebufferObject *m_fboEigenvalues;
    QOpenGLFramebufferObject *m_fboVesselness;
    QOpenGLFramebufferObject *m_fboOverlay;

    // Входная текстура
    QOpenGLTexture *m_inputTexture;
    QImage m_currentFrame;

    // Параметры фильтра
    float m_sigma;
    float m_beta;
    float m_c;
    
    // Какой stage показывать (0=grayscale, 1=invert, 2=blur, 3=gradients, 4=hessian, 5=eigenvalues, 6=vesselness, 7=overlay)
    int m_displayStage;
    
    // Включена ли инверсия
    bool m_invertEnabled;

    // Quad для рендеринга
    QOpenGLVertexArrayObject *m_vao;
    GLuint m_vbo;
};

#endif // FRANGIGLWIDGET_H