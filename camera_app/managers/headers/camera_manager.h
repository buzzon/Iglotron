#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief Информация о камере
 */
struct CameraInfo {
    int index;
    std::string name;
    int width;
    int height;
    double fps;
    bool available;
    
    CameraInfo() : index(-1), width(0), height(0), fps(0), available(false) {}
};

/**
 * @brief Менеджер камеры
 * 
 * Управляет обнаружением, открытием и захватом кадров с камеры
 */
class CameraManager {
public:
    CameraManager();
    ~CameraManager();
    
    /**
     * @brief Сканирование доступных камер
     * 
     * Проверяет первые 10 индексов камер и заполняет список доступных
     */
    void scanAvailableCameras();
    
    /**
     * @brief Получить список доступных камер
     * @return Константная ссылка на вектор камер
     */
    const std::vector<CameraInfo>& getAvailableCameras() const { return availableCameras; }
    
    /**
     * @brief Открыть камеру по индексу в списке availableCameras
     * @param index Индекс в списке availableCameras (не индекс камеры!)
     * @return true если успешно открыта
     */
    bool openCamera(int index);
    
    /**
     * @brief Открыть камеру по системному индексу
     * @param cameraIndex Системный индекс камеры
     * @return true если успешно открыта
     */
    bool openCameraBySystemIndex(int cameraIndex);
    
    /**
     * @brief Закрыть текущую камеру
     */
    void closeCamera();
    
    /**
     * @brief Проверить, открыта ли камера
     * @return true если камера открыта
     */
    bool isOpen() const { return isActive && capture.isOpened(); }
    
    /**
     * @brief Захватить кадр с камеры
     * @param output Выходной кадр
     * @return true если кадр успешно захвачен
     */
    bool grabFrame(cv::Mat& output);
    
    /**
     * @brief Установить разрешение камеры
     * @param width Ширина
     * @param height Высота
     */
    void setResolution(int width, int height);
    
    /**
     * @brief Установить FPS камеры
     * @param fps Частота кадров
     */
    void setFPS(double fps);
    
    /**
     * @brief Получить информацию о текущей камере
     * @return Информация о камере или пустая структура если не открыта
     */
    CameraInfo getCurrentCameraInfo() const;
    
    /**
     * @brief Получить индекс выбранной камеры в списке
     * @return Индекс в availableCameras
     */
    int getSelectedCameraIndex() const { return selectedCameraIndex; }
    
    /**
     * @brief Установить индекс выбранной камеры
     * @param index Индекс в availableCameras
     */
    void setSelectedCameraIndex(int index) { 
        if (index >= 0 && index < static_cast<int>(availableCameras.size())) {
            selectedCameraIndex = index;
        }
    }
    
    /**
     * @brief Переключиться на другую камеру
     * @param newIndex Новый индекс в availableCameras
     * @return true если переключение успешно
     */
    bool switchCamera(int newIndex);

private:
    cv::VideoCapture capture;
    std::vector<CameraInfo> availableCameras;
    int selectedCameraIndex;
    bool isActive;
    int currentSystemIndex;  // Системный индекс текущей камеры
    
    /**
     * @brief Применить настройки камеры (разрешение, FPS)
     */
    void applyCameraSettings();
};

#endif // CAMERA_MANAGER_H

