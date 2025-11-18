#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QVideoWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMediaCaptureSession>
#include <QCameraFormat>
#include <QMediaDevices>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onButton1Clicked();
    void onButton2Clicked();

private:
    QCamera *camera;
    QVideoWidget *videoWidget;
    QMediaCaptureSession *captureSession;
    QPushButton *button1;
    QPushButton *button2;
};

#endif // MAINWINDOW_H