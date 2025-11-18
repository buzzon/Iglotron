#include <QProcess>
#include "mainwindow.h"
#include <QMessageBox>
#include <QCameraFormat>
#include <QMediaDevices>
#include <QSize>
#include <QVideoFrame>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Создаем центральный виджет и основной layout
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Создаем горизонтальный layout для двух видео виджетов
    QHBoxLayout *videoLayout = new QHBoxLayout();
    
    // Виджет для исходного видео с камеры
    QVBoxLayout *rawLayout = new QVBoxLayout();
    QLabel *rawTitle = new QLabel("Raw Camera Feed", this);
    rawTitle->setAlignment(Qt::AlignCenter);
    rawLayout->addWidget(rawTitle);
    
    rawVideoLabel = new QLabel(this);
    rawVideoLabel->setMinimumSize(320, 240);
    rawVideoLabel->setScaledContents(true);
    rawVideoLabel->setStyleSheet("border: 2px solid green;");
    rawLayout->addWidget(rawVideoLabel);
    videoLayout->addLayout(rawLayout);
    
    // Виджет для обработанного видео (Frangi)
    QVBoxLayout *frangiLayout = new QVBoxLayout();
    QLabel *frangiTitle = new QLabel("Frangi Filter Output", this);
    frangiTitle->setAlignment(Qt::AlignCenter);
    frangiLayout->addWidget(frangiTitle);
    
    frangiWidget = new FrangiGLWidget(this);
    frangiWidget->setMinimumSize(320, 240);
    frangiWidget->setStyleSheet("border: 2px solid blue;");
    frangiLayout->addWidget(frangiWidget);
    videoLayout->addLayout(frangiLayout);
    
    mainLayout->addLayout(videoLayout);
    
    // Создаем панель управления параметрами
    QVBoxLayout *controlsLayout = new QVBoxLayout();
    
    // Sigma slider (0.5 - 10.0, default 1.5, step 0.01)
    QHBoxLayout *sigmaLayout = new QHBoxLayout();
    QLabel *sigmaTitle = new QLabel("Sigma (Scale):", this);
    sigmaSlider = new QSlider(Qt::Horizontal, this);
    sigmaSlider->setMinimum(50);   // 0.5 * 100
    sigmaSlider->setMaximum(1000); // 10.0 * 100
    sigmaSlider->setValue(150);    // 1.5 * 100
    sigmaLabel = new QLabel("1.50", this);
    sigmaLabel->setMinimumWidth(50);
    sigmaLayout->addWidget(sigmaTitle);
    sigmaLayout->addWidget(sigmaSlider);
    sigmaLayout->addWidget(sigmaLabel);
    controlsLayout->addLayout(sigmaLayout);
    
    // Beta (Alpha) slider (0.1 - 5.0, default 0.5, step 0.01)
    QHBoxLayout *betaLayout = new QHBoxLayout();
    QLabel *betaTitle = new QLabel("Alpha (Plate Sensitivity):", this);
    betaSlider = new QSlider(Qt::Horizontal, this);
    betaSlider->setMinimum(10);  // 0.1 * 100
    betaSlider->setMaximum(500); // 5.0 * 100
    betaSlider->setValue(50);    // 0.5 * 100
    betaLabel = new QLabel("0.50", this);
    betaLabel->setMinimumWidth(50);
    betaLayout->addWidget(betaTitle);
    betaLayout->addWidget(betaSlider);
    betaLayout->addWidget(betaLabel);
    controlsLayout->addLayout(betaLayout);
    
    // C (Gamma) slider (0.1 - 50.0, default 15.0, step 0.01)
    QHBoxLayout *cLayout = new QHBoxLayout();
    QLabel *cTitle = new QLabel("Gamma (Contrast):", this);
    cSlider = new QSlider(Qt::Horizontal, this);
    cSlider->setMinimum(10);    // 0.1 * 100
    cSlider->setMaximum(5000);  // 50.0 * 100
    cSlider->setValue(1500);    // 15.0 * 100
    cLabel = new QLabel("15.00", this);
    cLabel->setMinimumWidth(50);
    cLayout->addWidget(cTitle);
    cLayout->addWidget(cSlider);
    cLayout->addWidget(cLabel);
    controlsLayout->addLayout(cLayout);
    
    // Invert checkbox
    invertCheckBox = new QCheckBox("Enable Invert (for dark structures)", this);
    invertCheckBox->setChecked(true);  // По умолчанию включено
    controlsLayout->addWidget(invertCheckBox);
    
    // Display Stage selector
    QHBoxLayout *stageLayout = new QHBoxLayout();
    QLabel *stageTitle = new QLabel("Display Stage:", this);
    stageComboBox = new QComboBox(this);
    stageComboBox->addItem("0: Grayscale");
    stageComboBox->addItem("1: Invert");
    stageComboBox->addItem("2: Blur");
    stageComboBox->addItem("3: Gradients");
    stageComboBox->addItem("4: Hessian");
    stageComboBox->addItem("5: Eigenvalues");
    stageComboBox->addItem("6: Vesselness");
    stageComboBox->addItem("7: Overlay on Original");
    stageComboBox->setCurrentIndex(7);  // По умолчанию Overlay
    stageLayout->addWidget(stageTitle);
    stageLayout->addWidget(stageComboBox);
    controlsLayout->addLayout(stageLayout);
    
    mainLayout->addLayout(controlsLayout);
    
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
    
    // Подключаем сигналы ползунков, комбобокса и checkbox
    connect(sigmaSlider, &QSlider::valueChanged, this, &MainWindow::onSigmaChanged);
    connect(betaSlider, &QSlider::valueChanged, this, &MainWindow::onBetaChanged);
    connect(cSlider, &QSlider::valueChanged, this, &MainWindow::onCChanged);
    connect(stageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onStageChanged);
    connect(invertCheckBox, &QCheckBox::toggled, this, &MainWindow::onInvertToggled);
    
    // Подключаем сигналы кнопок (они ничего не делают, как и требовалось)
    connect(button1, &QPushButton::clicked, this, &MainWindow::onButton1Clicked);
    connect(button2, &QPushButton::clicked, this, &MainWindow::onButton2Clicked);
    
    // Получаем список доступных камер и выводим их
    QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    // QString cameraList;
    // for (int i = 0; i < devices.size(); ++i) {
    //     cameraList += QString("Камера %1: %2 (ID: %3)\n").arg(i).arg(devices[i].description()).arg(devices[i].id());
    // }


    // QMessageBox::information(this, "Доступные камеры", cameraList);

    // // Настраиваем камеру (специально выбираем USB20 Camera)
    QCameraDevice selectedDevice = devices [0];

    camera = new QCamera(selectedDevice, this);
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
    
    // Создаем video sink для получения кадров
    videoSink = new QVideoSink(this);
    connect(videoSink, &QVideoSink::videoFrameChanged,
            this, &MainWindow::onVideoFrameChanged);
    
    captureSession->setCamera(camera);
    captureSession->setVideoOutput(videoSink);
    
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

void MainWindow::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (frame.isValid()) {
        QVideoFrame clonedFrame(frame);
        if (clonedFrame.map(QVideoFrame::ReadOnly)) {
            QImage image = clonedFrame.toImage();
            clonedFrame.unmap();
            
            if (!image.isNull()) {
                // Показываем исходное изображение
                QPixmap pixmap = QPixmap::fromImage(image.scaled(
                    rawVideoLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                ));
                rawVideoLabel->setPixmap(pixmap);
                
                // Отправляем на обработку через Frangi
                frangiWidget->setFrame(image);
            }
        }
    }
}

void MainWindow::onSigmaChanged(int value)
{
    float sigma = value / 100.0f;
    sigmaLabel->setText(QString::number(sigma, 'f', 2));
    frangiWidget->setSigma(sigma);
}

void MainWindow::onBetaChanged(int value)
{
    float beta = value / 100.0f;
    betaLabel->setText(QString::number(beta, 'f', 2));
    frangiWidget->setBeta(beta);
}

void MainWindow::onCChanged(int value)
{
    float c = value / 100.0f;
    cLabel->setText(QString::number(c, 'f', 2));
    frangiWidget->setC(c);
}

void MainWindow::onStageChanged(int index)
{
    frangiWidget->setDisplayStage(index);
}

void MainWindow::onInvertToggled(bool checked)
{
    frangiWidget->setInvertEnabled(checked);
}