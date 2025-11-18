#include "mainwindow.h"
#include <QMessageBox>
#include <QCameraFormat>
#include <QMediaDevices>
#include <QSize>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Создаем центральный виджет и основной layout
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Создаем виджет для отображения видео
    videoWidget = new QVideoWidget(this);
    videoWidget->setMinimumSize(640, 480);
    mainLayout->addWidget(videoWidget);
    
    // Создаем layout для кнопок
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    
    button1 = new QPushButton("Кнопка 1", this);
    button2 = new QPushButton("Кнопка 2", this);
    
    button1->setMinimumSize(100, 40);
    button2->setMinimumSize(100, 40);
    
    buttonsLayout->addWidget(button1);
    buttonsLayout->addWidget(button2);
    buttonsLayout->addStretch();
    
    mainLayout->addLayout(buttonsLayout);
    
    setCentralWidget(centralWidget);
    
    // Подключаем сигналы кнопок (они ничего не делают, как и требовалось)
    connect(button1, &QPushButton::clicked, this, &MainWindow::onButton1Clicked);
    connect(button2, &QPushButton::clicked, this, &MainWindow::onButton2Clicked);
    
    // Настраиваем камеру
    camera = new QCamera(this);
    captureSession = new QMediaCaptureSession(this);
    
    // Настраиваем формат для минимальной задержки
    QCameraFormat bestFormat;
    int minResolution = INT_MAX;
    
    // Ищем формат с наименьшим разрешением и максимальным FPS для минимальной задержки
    for (const QCameraFormat &format : camera->cameraDevice().videoFormats()) {
        int resolution = format.resolution().width() * format.resolution().height();
        // Предпочитаем 640x480 или меньше, с высоким FPS
        if (resolution <= 640 * 480 && format.maxFrameRate() >= 30) {
            if (resolution < minResolution) {
                minResolution = resolution;
                bestFormat = format;
            }
        }
    }
    
    // Если нашли подходящий формат - применяем
    if (bestFormat.isNull()) {
        // Если не нашли, берем первый доступный
        auto formats = camera->cameraDevice().videoFormats();
        if (!formats.isEmpty()) {
            bestFormat = formats.first();
        }
    }
    
    if (!bestFormat.isNull()) {
        camera->setCameraFormat(bestFormat);
    }
    
    captureSession->setCamera(camera);
    captureSession->setVideoOutput(videoWidget);
    
    // Запускаем камеру
    camera->start();
}

MainWindow::~MainWindow()
{
    if (camera) {
        camera->stop();
    }
}

void MainWindow::onButton1Clicked()
{
    // Кнопка 1 ничего не делает, как и требовалось
}

void MainWindow::onButton2Clicked()
{
    // Кнопка 2 ничего не делает, как и требовалось
}