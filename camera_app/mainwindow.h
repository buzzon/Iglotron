#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMediaCaptureSession>
#include <QCameraFormat>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>
#include "frangiglwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onButton1Clicked();
    void onButton2Clicked();
    void onVideoFrameChanged(const QVideoFrame &frame);
    void onSigmaChanged(int value);
    void onBetaChanged(int value);
    void onCChanged(int value);
    void onStageChanged(int index);
    void onInvertToggled(bool checked);

private:
    QCamera *camera;
    FrangiGLWidget *frangiWidget;
    QLabel *rawVideoLabel;  // Для отображения исходного видео
    QMediaCaptureSession *captureSession;
    QVideoSink *videoSink;
    QPushButton *button1;
    QPushButton *button2;
    
    // Элементы управления параметрами
    QSlider *sigmaSlider;
    QSlider *betaSlider;
    QSlider *cSlider;
    QLabel *sigmaLabel;
    QLabel *betaLabel;
    QLabel *cLabel;
    QComboBox *stageComboBox;
    QCheckBox *invertCheckBox;
};

#endif // MAINWINDOW_H