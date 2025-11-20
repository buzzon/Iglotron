#include "camera_manager.h"
#include <iostream>

CameraManager::CameraManager() 
    : selectedCameraIndex(0), isActive(false), currentSystemIndex(-1) {
}

CameraManager::~CameraManager() {
    closeCamera();
}

void CameraManager::scanAvailableCameras() {
    availableCameras.clear();
    std::cout << "Scanning for available cameras..." << std::endl;
    
    // Проверяем первые 10 индексов камер
    for (int i = 0; i < 10; i++) {
        cv::VideoCapture testCap(i);
        
        if (testCap.isOpened()) {
            CameraInfo info;
            info.index = i;
            info.available = true;
            
            // Получаем параметры камеры
            info.width = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_WIDTH));
            info.height = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_HEIGHT));
            info.fps = testCap.get(cv::CAP_PROP_FPS);
            
            // Пробуем захватить кадр для проверки
            cv::Mat testFrame;
            testCap >> testFrame;
            
            if (!testFrame.empty()) {
                // Формируем имя камеры
                info.name = "Camera " + std::to_string(i) + " (" + 
                           std::to_string(info.width) + "x" + 
                           std::to_string(info.height) + ")";
                
                availableCameras.push_back(info);
                
                std::cout << "  ✓ Found: " << info.name 
                          << " @ " << info.fps << " FPS" << std::endl;
            }
            
            testCap.release();
        }
    }
    
    if (availableCameras.empty()) {
        std::cerr << "No cameras found!" << std::endl;
    } else {
        std::cout << "Found " << availableCameras.size() << " camera(s)" << std::endl;
    }
}

bool CameraManager::openCamera(int index) {
    // Проверяем валидность индекса
    if (index < 0 || index >= static_cast<int>(availableCameras.size())) {
        std::cerr << "Invalid camera index: " << index << std::endl;
        return false;
    }
    
    // Сканируем камеры если список пустой
    if (availableCameras.empty()) {
        scanAvailableCameras();
    }
    
    // Если камер нет, выходим
    if (availableCameras.empty()) {
        std::cerr << "No cameras available" << std::endl;
        return false;
    }
    
    // Закрываем предыдущую камеру если открыта
    if (isOpen()) {
        closeCamera();
    }
    
    // Открываем выбранную камеру
    int cameraIndex = availableCameras[index].index;
    selectedCameraIndex = index;
    currentSystemIndex = cameraIndex;
    
    std::cout << "Opening camera index: " << cameraIndex << std::endl;
    
    capture.open(cameraIndex);
    
    if (!capture.isOpened()) {
        std::cerr << "Failed to open camera " << cameraIndex << std::endl;
        isActive = false;
        return false;
    }
    
    // Применяем настройки
    applyCameraSettings();
    
    std::cout << "Camera " << cameraIndex << " initialized: " 
              << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << capture.get(cv::CAP_PROP_FRAME_HEIGHT) 
              << " @ " << capture.get(cv::CAP_PROP_FPS) << " FPS" << std::endl;
    
    isActive = true;
    return true;
}

bool CameraManager::openCameraBySystemIndex(int cameraIndex) {
    // Закрываем предыдущую камеру если открыта
    if (isOpen()) {
        closeCamera();
    }
    
    std::cout << "Opening camera by system index: " << cameraIndex << std::endl;
    
    capture.open(cameraIndex);
    
    if (!capture.isOpened()) {
        std::cerr << "Failed to open camera " << cameraIndex << std::endl;
        isActive = false;
        return false;
    }
    
    currentSystemIndex = cameraIndex;
    
    // Находим индекс в списке availableCameras
    selectedCameraIndex = -1;
    for (size_t i = 0; i < availableCameras.size(); i++) {
        if (availableCameras[i].index == cameraIndex) {
            selectedCameraIndex = static_cast<int>(i);
            break;
        }
    }
    
    // Применяем настройки
    applyCameraSettings();
    
    std::cout << "Camera " << cameraIndex << " initialized: " 
              << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << capture.get(cv::CAP_PROP_FRAME_HEIGHT) 
              << " @ " << capture.get(cv::CAP_PROP_FPS) << " FPS" << std::endl;
    
    isActive = true;
    return true;
}

void CameraManager::closeCamera() {
    if (capture.isOpened()) {
        capture.release();
    }
    isActive = false;
    currentSystemIndex = -1;
}

bool CameraManager::grabFrame(cv::Mat& output) {
    if (!isOpen()) {
        return false;
    }
    
    capture >> output;
    
    if (output.empty()) {
        std::cerr << "Failed to capture frame" << std::endl;
        return false;
    }
    
    return true;
}

void CameraManager::setResolution(int width, int height) {
    if (isOpen()) {
        capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }
}

void CameraManager::setFPS(double fps) {
    if (isOpen()) {
        capture.set(cv::CAP_PROP_FPS, fps);
    }
}

CameraInfo CameraManager::getCurrentCameraInfo() const {
    if (!isOpen() || selectedCameraIndex < 0 || 
        selectedCameraIndex >= static_cast<int>(availableCameras.size())) {
        return CameraInfo();
    }
    
    return availableCameras[selectedCameraIndex];
}

bool CameraManager::switchCamera(int newIndex) {
    if (newIndex < 0 || newIndex >= static_cast<int>(availableCameras.size())) {
        return false;
    }
    
    int previousIndex = selectedCameraIndex;
    
    std::cout << "Switching camera from index " 
              << (previousIndex >= 0 && previousIndex < static_cast<int>(availableCameras.size()) 
                  ? availableCameras[previousIndex].index : -1)
              << " to " << availableCameras[newIndex].index << std::endl;
    
    return openCamera(newIndex);
}

void CameraManager::applyCameraSettings() {
    if (!capture.isOpened()) {
        return;
    }
    
    // Настройки камеры для минимальной задержки
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);
}

